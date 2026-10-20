package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

type translationAction struct {
	ID           int         `json:"id"`
	Column       int         `json:"column"`
	Source       string      `json:"source"`
	SourceID     int         `json:"source_id"`
	SourceColumn int         `json:"source_column"`
	Text         *string     `json:"text,omitempty"`
	Replace      [][2]string `json:"replace,omitempty"`
}
type translationFile struct {
	Scope   string              `json:"scope"`
	Path    string              `json:"path"`
	SHA256  string              `json:"sha256"`
	Columns int                 `json:"columns"`
	Actions []translationAction `json:"actions"`
}
type translationPlan struct {
	Version int               `json:"version"`
	Files   []translationFile `json:"files"`
}
type textRecord struct {
	ID     int
	Fields [][]byte
	Spans  [][]int
}

func parseRecords(data []byte, columns int) []textRecord {
	pattern := `(?m)^\s*` + strings.Repeat(`\{([^}]*)\}\s*`, columns-1) + `\{([^}]*)\}`
	rx := regexp.MustCompile(pattern)
	var out []textRecord
	for _, spans := range rx.FindAllSubmatchIndex(data, -1) {
		id, err := strconv.Atoi(strings.TrimSpace(string(data[spans[2]:spans[3]])))
		if err != nil {
			continue
		}
		row := textRecord{ID: id}
		for c := 0; c < columns; c++ {
			a, b := spans[(c+1)*2], spans[(c+1)*2+1]
			row.Fields = append(row.Fields, data[a:b])
			row.Spans = append(row.Spans, []int{a, b})
		}
		out = append(out, row)
	}
	return out
}

var cp1252 = []rune{0x20ac, 0x81, 0x201a, 0x192, 0x201e, 0x2026, 0x2020, 0x2021, 0x2c6, 0x2030, 0x160, 0x2039, 0x152, 0x8d, 0x17d, 0x8f, 0x90, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x2dc, 0x2122, 0x161, 0x203a, 0x153, 0x9d, 0x17e, 0x178}

func encodeGame(s string) ([]byte, error) {
	var out []byte
	for _, r := range s {
		if r < 128 || r >= 160 && r <= 255 {
			out = append(out, byte(r))
			continue
		}
		found := false
		for i, c := range cp1252 {
			if c == r {
				out = append(out, byte(i+128))
				found = true
				break
			}
		}
		if !found {
			return nil, fmt.Errorf("unsupported French character: %U", r)
		}
	}
	return out, nil
}
func localize(data []byte, plan translationFile, read func(string) ([]byte, error)) ([]byte, error) {
	sum := sha256.Sum256(data)
	if hex.EncodeToString(sum[:]) != plan.SHA256 {
		return nil, fmt.Errorf("unexpected UAP resource: %s", plan.Path)
	}
	actions := map[[2]int]translationAction{}
	for _, a := range plan.Actions {
		key := [2]int{a.ID, a.Column}
		if _, ok := actions[key]; ok {
			return nil, fmt.Errorf("duplicate translation action")
		}
		actions[key] = a
	}
	cache := map[string]map[int]textRecord{}
	var result bytes.Buffer
	at := 0
	applied := 0
	for _, row := range parseRecords(data, plan.Columns) {
		for col := 1; col < plan.Columns; col++ {
			a, ok := actions[[2]int{row.ID, col}]
			if !ok {
				continue
			}
			if plan.Columns == 7 && (col > 2 || col == 2 && strings.TrimSpace(string(row.Fields[3])) != "" && strings.TrimSpace(string(row.Fields[3])) != "0") {
				return nil, fmt.Errorf("translation touches dialog logic: %s:%d", plan.Path, row.ID)
			}
			var value []byte
			var err error
			if a.Text != nil {
				value, err = encodeGame(*a.Text)
			} else {
				records, exists := cache[a.Source]
				if !exists {
					var raw []byte
					raw, err = read(a.Source)
					if err != nil {
						return nil, err
					}
					columns := 2
					if strings.HasSuffix(a.Source, ".dlg") {
						columns = 7
					}
					records = map[int]textRecord{}
					for _, r := range parseRecords(raw, columns) {
						records[r.ID] = r
					}
					cache[a.Source] = records
				}
				r, exists := records[a.SourceID]
				if !exists || a.SourceColumn < 1 || a.SourceColumn >= len(r.Fields) {
					return nil, fmt.Errorf("missing French reference: %s:%d", a.Source, a.SourceID)
				}
				value = r.Fields[a.SourceColumn]
			}
			if err != nil {
				return nil, err
			}
			if len(bytes.TrimSpace(value)) == 0 {
				return nil, fmt.Errorf("empty French translation: %s:%d", plan.Path, row.ID)
			}
			if plan.Path == "mes/gamearea.mes" {
				original := bytes.SplitN(row.Fields[col], []byte("/"), 2)
				translated := bytes.SplitN(value, []byte("/"), 2)
				if len(original) != 2 || len(translated) != 2 {
					return nil, fmt.Errorf("invalid area record: %d", row.ID)
				}
				value = append(append(append([]byte{}, original[0]...), '/'), translated[1]...)
			}
			for _, edit := range a.Replace {
				from, e := encodeGame(edit[0])
				if e != nil {
					return nil, e
				}
				to, e := encodeGame(edit[1])
				if e != nil {
					return nil, e
				}
				if !bytes.Contains(value, from) {
					return nil, fmt.Errorf("missing French replacement: %s:%d", plan.Path, row.ID)
				}
				value = bytes.ReplaceAll(value, from, to)
			}
			if bytes.ContainsAny(value, "{}") {
				return nil, fmt.Errorf("invalid translation braces")
			}
			span := row.Spans[col]
			result.Write(data[at:span[0]])
			result.Write(value)
			at = span[1]
			applied++
		}
	}
	if applied != len(actions) {
		return nil, fmt.Errorf("unapplied translation actions: %s", plan.Path)
	}
	result.Write(data[at:])
	return result.Bytes(), nil
}
func buildFrench(game, extracted, output, planPath string) error {
	raw, err := os.ReadFile(planPath)
	if err != nil {
		return err
	}
	var plan translationPlan
	if err = json.Unmarshal(raw, &plan); err != nil {
		return err
	}
	if plan.Version != 1 {
		return fmt.Errorf("unsupported translation plan")
	}
	fr, err := archiveSet(game)
	if err != nil {
		return err
	}
	modules, err := findCase(game, "modules")
	if err != nil {
		return err
	}
	for _, name := range []string{"Arcanum.dat", "Arcanum.patch0"} {
		p, e := findCase(modules, name)
		if os.IsNotExist(e) {
			continue
		}
		if e != nil {
			return e
		}
		a, e := openDAT(p)
		if e != nil {
			return e
		}
		fr = append(fr, a)
	}
	var base []*datArchive
	for _, name := range []string{"Arcanum5.dat", "ArcanumXRaces.dat"} {
		p, e := findCase(extracted, name)
		if e != nil {
			return e
		}
		a, e := openDAT(p)
		if e != nil {
			return e
		}
		base = append(base, a)
	}
	mp, err := findCase(extracted, "modules/Arcanum.patch1")
	if err != nil {
		return err
	}
	module, err := openDAT(mp)
	if err != nil {
		return err
	}
	outputs := map[string]map[string][]byte{"global": {}, "module": {}}
	for _, file := range plan.Files {
		source := base
		if file.Scope == "module" {
			source = []*datArchive{module}
		}
		raw, err := readResource(source, file.Path)
		if err != nil {
			return err
		}
		translated, err := localize(raw, file, func(name string) ([]byte, error) { return readResource(fr, name) })
		if err != nil {
			return err
		}
		outputs[file.Scope][file.Path] = translated
	}
	for scope, files := range outputs {
		if err = writeDAT(filepath.Join(output, "french-"+scope+".dat"), files); err != nil {
			return err
		}
	}
	return nil
}
func findCase(root, relative string) (string, error) {
	current := root
	for _, part := range strings.Split(canonical(relative), "/") {
		if part == "" || part == "." || part == ".." {
			return "", fmt.Errorf("invalid relative path")
		}
		entries, err := os.ReadDir(current)
		if err != nil {
			return "", err
		}
		match := ""
		for _, e := range entries {
			if strings.EqualFold(e.Name(), part) {
				if match != "" {
					return "", fmt.Errorf("ambiguous file case: %s", relative)
				}
				match = e.Name()
			}
		}
		if match == "" {
			return "", os.ErrNotExist
		}
		current = filepath.Join(current, match)
	}
	return current, nil
}
