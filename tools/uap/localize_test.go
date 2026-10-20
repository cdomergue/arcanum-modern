package main

import (
	"crypto/sha256"
	"fmt"
	"strings"
	"testing"
)

func TestLocalizationPreservesLogic(t *testing.T) {
	raw := []byte("{10}{English}{Female}{}{ra9, gf13 0}{22}{gf13 1}\r\n{11}{Response}{1}{5}{ra10}{0}{}\r\n")
	fr := []byte("{10}{Fran\xe7ais}{F\xe9minin}{}{old}{0}{old}\n")
	word := "Réponse"
	p := translationFile{Path: "dlg/test.dlg", SHA256: fmt.Sprintf("%x", sha256.Sum256(raw)), Columns: 7, Actions: []translationAction{{ID: 10, Column: 1, Source: "test.dlg", SourceID: 10, SourceColumn: 1}, {ID: 10, Column: 2, Source: "test.dlg", SourceID: 10, SourceColumn: 2}, {ID: 11, Column: 1, Text: &word}}}
	out, err := localize(raw, p, func(string) ([]byte, error) { return fr, nil })
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(out), "{ra9, gf13 0}{22}{gf13 1}") || !strings.Contains(string(out), "{R\xe9ponse}{1}{5}{ra10}{0}{}") {
		t.Fatal(string(out))
	}
	p.Actions = append(p.Actions, translationAction{ID: 11, Column: 2, Text: &word})
	if _, err = localize(raw, p, func(string) ([]byte, error) { return fr, nil }); err == nil {
		t.Fatal("logic modification accepted")
	}
}

func TestIndentedDialogueRows(t *testing.T) {
	data := []byte("{1}{Hello}{}{0}{}{0}{}\r\n  {2}{Next}{}{0}{}{0}{}\r\n\t{3}{Last}{}{0}{}{0}{}")
	rows := parseRecords(data, 7)
	if len(rows) != 3 || rows[1].ID != 2 {
		t.Fatal("indented row lost", rows)
	}
}
func TestAreaCoordinatesRemainUAP(t *testing.T) {
	raw := []byte("{21}{62182,65662, 0, 0 /Tarant/City}")
	french := []byte("{21}{62243,65664, 0, 0 /Tarante/Ville}")
	p := translationFile{Path: "mes/gamearea.mes", SHA256: fmt.Sprintf("%x", sha256.Sum256(raw)), Columns: 2, Actions: []translationAction{{ID: 21, Column: 1, Source: "mes/gamearea.mes", SourceID: 21, SourceColumn: 1}}}
	out, err := localize(raw, p, func(string) ([]byte, error) { return french, nil })
	if err != nil {
		t.Fatal(err)
	}
	if string(out) != "{21}{62182,65662, 0, 0 /Tarante/Ville}" {
		t.Fatal(string(out))
	}
}
