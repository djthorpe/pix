package main

import (
	"encoding/xml"
	"errors"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

// --- SVG model ---
type svgFile struct {
	XMLName xml.Name  `xml:"svg"`
	ViewBox string    `xml:"viewBox,attr"`
	Paths   []svgPath `xml:"path"`
}
type svgPath struct {
	D string `xml:"d,attr"`
}

// --- Geometry ---
type fpoint struct{ x, y float64 }
type subpath struct{ pts []fpoint }

// --- CLI ---
func main() {
	in := flag.String("in", "", "Input SVG file or directory")
	outDir := flag.String("out", "generated", "Output directory for C sources")
	prefix := flag.String("prefix", "vg_icon_", "Symbol name prefix")
	size := flag.Int("size", 0, "Normalize to square size (0 = keep viewBox)")
	flatness := flag.Float64("flatness", 0.25, "Curve flattening tolerance (px)")
	strategy := flag.String("strategy", "array", "Emission strategy: array|calls")
	limit := flag.Int("limit", 0, "Process only first N files (debug)")
	flag.Parse()
	if *in == "" {
		fmt.Fprintln(os.Stderr, "--in required")
		os.Exit(1)
	}
	if err := run(*in, *outDir, *prefix, *size, *flatness, *strategy, *limit); err != nil {
		fmt.Fprintln(os.Stderr, "error:", err)
		os.Exit(1)
	}
}

func run(inPath, outDir, prefix string, normSize int, flat float64, strategy string, limit int) error {
	fi, err := os.Stat(inPath)
	if err != nil {
		return err
	}
	var files []string
	if fi.IsDir() {
		err = filepath.WalkDir(inPath, func(p string, d os.DirEntry, e error) error {
			if e != nil {
				return e
			}
			if d.IsDir() {
				return nil
			}
			if strings.HasSuffix(strings.ToLower(d.Name()), ".svg") {
				files = append(files, p)
			}
			return nil
		})
		if err != nil {
			return err
		}
	} else {
		files = []string{inPath}
	}
	if len(files) == 0 {
		return errors.New("no svg files found")
	}
	sort.Strings(files)
	if limit > 0 && len(files) > limit {
		files = files[:limit]
	}
	if err := os.MkdirAll(outDir, 0o755); err != nil {
		return err
	}

	hdrEntries := make([]string, 0, len(files))
	for _, f := range files {
		iconName := strings.TrimSuffix(filepath.Base(f), filepath.Ext(f))
		data, err := os.ReadFile(f)
		if err != nil {
			return err
		}
		svg, err := parseSVG(data)
		if err != nil {
			return fmt.Errorf("%s: %w", f, err)
		}
		subpaths, w, h, warn := buildGeometry(svg, normSize, flat)
		csrc := emitC(iconName, prefix, strategy, subpaths, int(w), int(h), warn)
		if err := os.WriteFile(filepath.Join(outDir, iconName+".c"), []byte(csrc), 0o644); err != nil {
			return err
		}
		hdrEntries = append(hdrEntries, iconName)
	}
	hdr := emitHeader(hdrEntries, prefix)
	return os.WriteFile(filepath.Join(outDir, "icons.h"), []byte(hdr), 0o644)
}

// --- Parsing SVG/XML ---
func parseSVG(b []byte) (*svgFile, error) {
	dec := xml.NewDecoder(strings.NewReader(string(b)))
	dec.Strict = false
	for {
		tok, err := dec.Token()
		if err == io.EOF {
			break
		} else if err != nil {
			return nil, err
		}
		if se, ok := tok.(xml.StartElement); ok && se.Name.Local == "svg" {
			var s svgFile
			if err := dec.DecodeElement(&s, &se); err != nil {
				return nil, err
			}
			return &s, nil
		}
	}
	return nil, errors.New("no <svg> root element")
}

// --- Path data tokenization ---
type lexer struct {
	s string
	i int
}

func (l *lexer) skipWS() {
	for l.i < len(l.s) {
		c := l.s[l.i]
		if c == ' ' || c == '\n' || c == '\t' || c == ',' {
			l.i++
			continue
		}
		break
	}
}
func (l *lexer) peek() rune {
	if l.i >= len(l.s) {
		return 0
	}
	return rune(l.s[l.i])
}
func (l *lexer) next() rune {
	if l.i >= len(l.s) {
		return 0
	}
	r := rune(l.s[l.i])
	l.i++
	return r
}
func (l *lexer) number() (float64, bool) {
	l.skipWS()
	start := l.i
	seen := false
	if l.peek() == '+' || l.peek() == '-' {
		l.i++
	}
	for {
		c := l.peek()
		if c >= '0' && c <= '9' {
			l.i++
			seen = true
		} else {
			break
		}
	}
	if l.peek() == '.' {
		l.i++
		for {
			c := l.peek()
			if c >= '0' && c <= '9' {
				l.i++
				seen = true
			} else {
				break
			}
		}
	}
	if l.peek() == 'e' || l.peek() == 'E' {
		l.i++
		if l.peek() == '+' || l.peek() == '-' {
			l.i++
		}
		for {
			c := l.peek()
			if c >= '0' && c <= '9' {
				l.i++
				seen = true
			} else {
				break
			}
		}
	}
	if !seen {
		return 0, false
	}
	v, err := strconv.ParseFloat(l.s[start:l.i], 64)
	if err != nil {
		return 0, false
	}
	return v, true
}

// --- Path command parsing & flattening ---
func buildGeometry(svg *svgFile, normSize int, flat float64) ([]subpath, float64, float64, []string) {
	vbMinX, vbMinY, vbW, vbH := 0.0, 0.0, 24.0, 24.0
	if svg.ViewBox != "" {
		parts := strings.Fields(svg.ViewBox)
		if len(parts) == 4 {
			vbMinX, _ = strconv.ParseFloat(parts[0], 64)
			vbMinY, _ = strconv.ParseFloat(parts[1], 64)
			vbW, _ = strconv.ParseFloat(parts[2], 64)
			vbH, _ = strconv.ParseFloat(parts[3], 64)
		}
	}
	scale := 1.0
	outW, outH := vbW, vbH
	if normSize > 0 && (vbW > 0 && vbH > 0) {
		scale = float64(normSize) / math.Max(vbW, vbH)
		outW = vbW * scale
		outH = vbH * scale
	}
	var subpaths []subpath
	var warnings []string
	for _, p := range svg.Paths {
		spaths, warn := parsePathData(p.D, flat, func(x, y float64) (float64, float64) {
			return (x - vbMinX) * scale, (y - vbMinY) * scale
		})
		if len(spaths) > 0 {
			subpaths = append(subpaths, spaths...)
		}
		warnings = append(warnings, warn...)
	}
	return subpaths, outW, outH, warnings
}

func parsePathData(d string, flat float64, xf func(x, y float64) (float64, float64)) ([]subpath, []string) {
	l := lexer{s: d}
	var cur fpoint
	var start fpoint
	var subs []subpath
	var curSub *subpath
	var prevCmd rune
	var reflC, reflQ *fpoint
	addPoint := func(pt fpoint) {
		if curSub == nil {
			curSub = &subpath{}
			subs = append(subs, *curSub)
			curSub = &subs[len(subs)-1]
		}
		// de-dup
		if n := len(curSub.pts); n > 0 {
			last := curSub.pts[n-1]
			if math.Abs(last.x-pt.x) < 1e-6 && math.Abs(last.y-pt.y) < 1e-6 {
				return
			}
		}
		curSub.pts = append(curSub.pts, pt)
	}
	closeSub := func() {
		if curSub != nil && len(curSub.pts) > 1 {
			first := curSub.pts[0]
			last := curSub.pts[len(curSub.pts)-1]
			if math.Abs(first.x-last.x) > 1e-6 || math.Abs(first.y-last.y) > 1e-6 {
				curSub.pts = append(curSub.pts, first)
			}
		}
		curSub = nil
	}
	warnings := []string{}
	for {
		l.skipWS()
		if l.i >= len(l.s) {
			break
		}
		c := l.peek()
		if (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' { // implicit command repeat
			c = prevCmd
		} else {
			c = l.next()
			prevCmd = c
		}
		abs := c >= 'A' && c <= 'Z'
		cmd := unicodeLower(c)
		switch cmd {
		case 'm':
			x1, ok1 := l.number()
			y1, ok2 := l.number()
			if !ok1 || !ok2 {
				return subs, append(warnings, "bad moveto")
			}
			if !abs {
				x1 += cur.x
				y1 += cur.y
			}
			cur = fpoint{x1, y1}
			start = cur
			if curSub != nil {
				closeSub()
			}
			curSub = &subpath{}
			subs = append(subs, *curSub)
			curSub = &subs[len(subs)-1]
			addPoint(transformPoint(cur, xf))
			reflC = nil
			reflQ = nil
			prevCmd = 'l' // subsequent pairs are lineto
		case 'l':
			x1, ok1 := l.number()
			y1, ok2 := l.number()
			if !ok1 || !ok2 {
				warnings = append(warnings, "bad lineto")
				continue
			}
			if !abs {
				x1 += cur.x
				y1 += cur.y
			}
			cur = fpoint{x1, y1}
			addPoint(transformPoint(cur, xf))
			reflC = nil
			reflQ = nil
		case 'h':
			x1, ok := l.number()
			if !ok {
				warnings = append(warnings, "bad h")
				continue
			}
			if !abs {
				x1 += cur.x
			}
			cur = fpoint{x1, cur.y}
			addPoint(transformPoint(cur, xf))
			reflC = nil
			reflQ = nil
		case 'v':
			y1, ok := l.number()
			if !ok {
				warnings = append(warnings, "bad v")
				continue
			}
			if !abs {
				y1 += cur.y
			}
			cur = fpoint{cur.x, y1}
			addPoint(transformPoint(cur, xf))
			reflC = nil
			reflQ = nil
		case 'z':
			closeSub()
			cur = start
			reflC = nil
			reflQ = nil
		case 'c':
			x1, o1 := l.number()
			y1, o2 := l.number()
			x2, o3 := l.number()
			y2, o4 := l.number()
			x3, o5 := l.number()
			y3, o6 := l.number()
			if !(o1 && o2 && o3 && o4 && o5 && o6) {
				warnings = append(warnings, "bad c")
				continue
			}
			if !abs {
				x1 += cur.x
				y1 += cur.y
				x2 += cur.x
				y2 += cur.y
				x3 += cur.x
				y3 += cur.y
			}
			pts := flattenCubic(cur, fpoint{x1, y1}, fpoint{x2, y2}, fpoint{x3, y3}, flat)
			for i := 1; i < len(pts); i++ {
				addPoint(transformPoint(pts[i], xf))
			}
			cur = fpoint{x3, y3}
			reflC = &fpoint{x: 2*cur.x - x2, y: 2*cur.y - y2}
			reflQ = nil
		case 's':
			x2, o1 := l.number()
			y2, o2 := l.number()
			x3, o3 := l.number()
			y3, o4 := l.number()
			if !(o1 && o2 && o3 && o4) {
				warnings = append(warnings, "bad s")
				continue
			}
			if reflC == nil {
				reflC = &cur
			}
			cx1, cy1 := reflC.x, reflC.y
			if !abs {
				x2 += cur.x
				y2 += cur.y
				x3 += cur.x
				y3 += cur.y
			}
			pts := flattenCubic(cur, fpoint{cx1, cy1}, fpoint{x2, y2}, fpoint{x3, y3}, flat)
			for i := 1; i < len(pts); i++ {
				addPoint(transformPoint(pts[i], xf))
			}
			cur = fpoint{x3, y3}
			reflC = &fpoint{x: 2*cur.x - x2, y: 2*cur.y - y2}
			reflQ = nil
		case 'q':
			x1, o1 := l.number()
			y1, o2 := l.number()
			x2, o3 := l.number()
			y2, o4 := l.number()
			if !(o1 && o2 && o3 && o4) {
				warnings = append(warnings, "bad q")
				continue
			}
			if !abs {
				x1 += cur.x
				y1 += cur.y
				x2 += cur.x
				y2 += cur.y
			}
			pts := flattenQuadratic(cur, fpoint{x1, y1}, fpoint{x2, y2}, flat)
			for i := 1; i < len(pts); i++ {
				addPoint(transformPoint(pts[i], xf))
			}
			cur = fpoint{x2, y2}
			reflQ = &fpoint{x: 2*cur.x - x1, y: 2*cur.y - y1}
			reflC = nil
		case 't':
			x2, o1 := l.number()
			y2, o2 := l.number()
			if !(o1 && o2) {
				warnings = append(warnings, "bad t")
				continue
			}
			if reflQ == nil {
				reflQ = &cur
			}
			ctrl := *reflQ
			if !abs {
				x2 += cur.x
				y2 += cur.y
			}
			pts := flattenQuadratic(cur, ctrl, fpoint{x2, y2}, flat)
			for i := 1; i < len(pts); i++ {
				addPoint(transformPoint(pts[i], xf))
			}
			cur = fpoint{x2, y2}
			reflQ = &fpoint{x: 2*cur.x - ctrl.x, y: 2*cur.y - ctrl.y}
			reflC = nil
		case 'a':
			rx, o1 := l.number()
			ry, o2 := l.number()
			rot, o3 := l.number()
			laf, o4 := l.number()
			sf, o5 := l.number()
			x2, o6 := l.number()
			y2, o7 := l.number()
			if !(o1 && o2 && o3 && o4 && o5 && o6 && o7) {
				warnings = append(warnings, "bad a")
				continue
			}
			if !abs {
				x2 += cur.x
				y2 += cur.y
			}
			pts := flattenArc(cur, fpoint{x2, y2}, rx, ry, rot*math.Pi/180.0, laf != 0, sf != 0, flat)
			for i := 1; i < len(pts); i++ {
				addPoint(transformPoint(pts[i], xf))
			}
			cur = fpoint{x2, y2}
			reflC = nil
			reflQ = nil
		default:
			warnings = append(warnings, fmt.Sprintf("unsupported cmd %c", c))
		}
	}
	if curSub != nil {
		closeSub()
	}
	return subs, warnings
}

func transformPoint(p fpoint, xf func(x, y float64) (float64, float64)) fpoint {
	x, y := xf(p.x, p.y)
	return fpoint{x, y}
}

func flattenCubic(p0, p1, p2, p3 fpoint, flat float64) []fpoint {
	var out []fpoint
	var rec func(a, b, c, d fpoint, depth int)
	tol2 := flat * flat
	rec = func(a, b, c, d fpoint, depth int) {
		// approximate flatness via control net hull
		d1 := dist2PointLine(b, a, d)
		d2 := dist2PointLine(c, a, d)
		if (d1 <= tol2 && d2 <= tol2) || depth > 12 {
			out = append(out, d)
			return
		}
		ab := mid(a, b)
		bc := mid(b, c)
		cd := mid(c, d)
		abc := mid(ab, bc)
		bcd := mid(bc, cd)
		abcd := mid(abc, bcd)
		rec(a, ab, abc, abcd, depth+1)
		rec(abcd, bcd, cd, d, depth+1)
	}
	out = append(out, p0)
	rec(p0, p1, p2, p3, 0)
	return out
}
func flattenQuadratic(p0, p1, p2 fpoint, flat float64) []fpoint {
	// Convert to cubic by elevating degree or implement similar recursion
	c1 := fpoint{p0.x + 2.0/3.0*(p1.x-p0.x), p0.y + 2.0/3.0*(p1.y-p0.y)}
	c2 := fpoint{p2.x + 2.0/3.0*(p1.x-p2.x), p2.y + 2.0/3.0*(p1.y-p2.y)}
	return flattenCubic(p0, c1, c2, p2, flat)
}
func flattenArc(p0, p1 fpoint, rx, ry, phi float64, large, sweep bool, flat float64) []fpoint {
	// Implementation adapted from SVG spec algorithm (simplified)
	if rx == 0 || ry == 0 {
		return []fpoint{p0, p1}
	}
	rx = math.Abs(rx)
	ry = math.Abs(ry)
	cosPhi := math.Cos(phi)
	sinPhi := math.Sin(phi)
	dx2 := (p0.x - p1.x) / 2.0
	dy2 := (p0.y - p1.y) / 2.0
	x1p := cosPhi*dx2 + sinPhi*dy2
	y1p := -sinPhi*dx2 + cosPhi*dy2
	l := (x1p*x1p)/(rx*rx) + (y1p*y1p)/(ry*ry)
	if l > 1 {
		s := math.Sqrt(l)
		rx *= s
		ry *= s
	}
	sign := 1.0
	if large == sweep {
		sign = -1
	}
	num := rx*rx*ry*ry - rx*rx*y1p*y1p - ry*ry*x1p*x1p
	if num < 0 {
		num = 0
	}
	cfac := sign * math.Sqrt(num/(rx*rx*y1p*y1p+ry*ry*x1p*x1p))
	cxp := cfac * (rx * y1p) / ry
	cyp := cfac * (-ry * x1p) / rx
	cx := cosPhi*cxp - sinPhi*cyp + (p0.x+p1.x)/2.0
	cy := sinPhi*cxp + cosPhi*cyp + (p0.y+p1.y)/2.0
	theta1 := angle(1, 0, (x1p-cxp)/rx, (y1p-cyp)/ry)
	dtheta := angle((x1p-cxp)/rx, (y1p-cyp)/ry, (-x1p-cxp)/rx, (-y1p-cyp)/ry)
	if !sweep && dtheta > 0 {
		dtheta -= 2 * math.Pi
	} else if sweep && dtheta < 0 {
		dtheta += 2 * math.Pi
	}
	// Subdivide arc
	segs := int(math.Ceil(math.Abs(dtheta) / (2 * math.Acos(1-flat/(math.Max(rx, ry)+1e-9)))))
	if segs < 1 {
		segs = 1
	}
	dt := dtheta / float64(segs)
	pts := []fpoint{p0}
	for i := 1; i <= segs; i++ {
		t := theta1 + dt*float64(i)
		x := cx + rx*math.Cos(t)*cosPhi - ry*math.Sin(t)*sinPhi
		y := cy + rx*math.Cos(t)*sinPhi + ry*math.Sin(t)*cosPhi
		pts = append(pts, fpoint{x, y})
	}
	return pts
}
func angle(ux, uy, vx, vy float64) float64 {
	dot := ux*vx + uy*vy
	det := ux*vy - uy*vx
	a := math.Atan2(det, dot)
	return a
}
func dist2PointLine(p, a, b fpoint) float64 { // squared distance from p to line ab
	px, py := p.x, p.y
	x1, y1 := a.x, a.y
	x2, y2 := b.x, b.y
	num := math.Abs((y2-y1)*px - (x2-x1)*py + x2*y1 - y2*x1)
	den := math.Hypot(y2-y1, x2-x1)
	if den == 0 {
		return 0
	}
	d := num / den
	return d * d
}
func mid(a, b fpoint) fpoint { return fpoint{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0} }
func unicodeLower(r rune) rune {
	if r >= 'A' && r <= 'Z' {
		return r + ('a' - 'A')
	}
	return r
}

// --- Emission ---
func emitC(name, prefix, strategy string, subs []subpath, w, h int, warnings []string) string {
	sym := prefix + sanitizeIdent(name)
	var b strings.Builder
	b.WriteString("// Generated icon: " + name + " (" + fmt.Sprint(w, "x", h) + ")\n")
	for _, w := range warnings {
		b.WriteString("// WARNING: " + w + "\n")
	}
	b.WriteString("#include <vg/shape.h>\n#include <vg/primitives.h>\n\n")
	// Flatten all points
	var total int
	for _, sp := range subs {
		total += len(sp.pts)
	}
	if strategy == "array" {
		b.WriteString("static const pix_point_t " + sym + "_pts[] = {\n")
		for _, sp := range subs {
			for _, pt := range sp.pts {
				b.WriteString(fmt.Sprintf("  { %d, %d },\n", clamp16(pt.x), clamp16(pt.y)))
			}
		}
		b.WriteString("};\n")
		b.WriteString("static const uint16_t " + sym + "_offs[] = {\n")
		off := 0
		for _, sp := range subs {
			b.WriteString(fmt.Sprintf("  %d,\n", off))
			off += len(sp.pts)
		}
		b.WriteString("};\n\n")
		b.WriteString("bool " + sym + "(vg_shape_t *s) {\n  if(!s) return false;\n  if(!vg_shape_path_clear(s, " + fmt.Sprint(total) + ")) return false;\n  vg_path_t *p = vg_shape_path(s); if(!p) return false;\n  int idx=0;\n")
		off = 0
		for _, sp := range subs {
			b.WriteString("  // subpath\n")
			for range sp.pts {
				b.WriteString(fmt.Sprintf("  vg_path_append(p, &%s_pts[idx++], NULL);\n", sym))
			}
		}
		b.WriteString("  return true;\n}\n")
	} else { // calls
		b.WriteString("bool " + sym + "(vg_shape_t *s) {\n  if(!s) return false;\n  size_t reserve = " + fmt.Sprint(total) + ";\n  if(!vg_shape_path_clear(s, reserve)) return false;\n  vg_path_t *p = vg_shape_path(s); if(!p) return false;\n")
		for _, sp := range subs {
			b.WriteString("  // subpath\n")
			// batch points in groups of up to 16 per variadic call
			batch := []string{}
			flush := func() {
				if len(batch) == 0 {
					return
				}
				b.WriteString("  vg_path_append(p, " + strings.Join(batch, ", ") + ", NULL);\n")
				batch = batch[:0]
			}
			for _, pt := range sp.pts {
				lit := fmt.Sprintf("&(pix_point_t){%d,%d}", clamp16(pt.x), clamp16(pt.y))
				batch = append(batch, lit)
				if len(batch) >= 16 {
					flush()
				}
			}
			flush()
		}
		b.WriteString("  return true;\n}\n")
	}
	return b.String()
}

func emitHeader(names []string, prefix string) string {
	var b strings.Builder
	b.WriteString("// Generated icons header\n#pragma once\n#include <vg/shape.h>\n\n")
	for _, n := range names {
		b.WriteString("bool " + prefix + sanitizeIdent(n) + "(vg_shape_t *s);\n")
	}
	return b.String()
}

func clamp16(f float64) int16 {
	if f > 32767 {
		return 32767
	}
	if f < -32768 {
		return -32768
	}
	return int16(math.Round(f))
}
func sanitizeIdent(s string) string {
	out := make([]rune, 0, len(s))
	for i, r := range s {
		if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || r == '_' || (i > 0 && r >= '0' && r <= '9') {
			out = append(out, r)
		} else if r == '-' || r == ' ' || r == '.' {
			out = append(out, '_')
		}
	}
	if len(out) == 0 {
		return "icon"
	}
	return string(out)
}
