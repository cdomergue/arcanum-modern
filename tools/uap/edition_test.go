package main

import (
	"os"
	"path/filepath"
	"testing"
)

func editionFixture(t *testing.T) (string, catalog) {
	t.Helper()
	root := t.TempDir()
	c := catalog{Original: map[string]backupEntry{}}
	for _, name := range []string{"arcanum1.dat", "arcanum2.dat", "arcanum3.dat", "arcanum4.dat", "modules/arcanum.dat", "modules/arcanum.patch0"} {
		path := filepath.Join(root, name)
		if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
			t.Fatal(err)
		}
		if err := writeDAT(path, map[string][]byte{"fixture": []byte(name)}); err != nil {
			t.Fatal(err)
		}
		hash, size, err := fileHash(path)
		if err != nil {
			t.Fatal(err)
		}
		c.Original[name] = backupEntry{size, hash}
	}
	return root, c
}
func putLanguage(t *testing.T, root, text string) {
	t.Helper()
	if err := writeDAT(filepath.Join(root, "arcanum3.dat"), map[string][]byte{"mes/intgame.mes": []byte("{0}{" + text + "}\n{1}{Reaction}\n")}); err != nil {
		t.Fatal(err)
	}
}
func TestEditionDetection(t *testing.T) {
	root, c := editionFixture(t)
	if lang, err := detectEdition(root, c); err != nil || lang != "fr" {
		t.Fatalf("French fingerprint: %s %v", lang, err)
	}
	putLanguage(t, root, "Level")
	if lang, err := detectEdition(root, c); err != nil || lang != "en" {
		t.Fatalf("English text: %s %v", lang, err)
	}
	for _, label := range []string{"Niveau", "Stufe", "", "Level up"} {
		putLanguage(t, root, label)
		if _, err := detectEdition(root, c); err == nil {
			t.Fatalf("accepted %q as English", label)
		}
	}
	putLanguage(t, root, "Level")
	loose := filepath.Join(root, "data", "mes", "intgame.mes")
	os.MkdirAll(filepath.Dir(loose), 0700)
	os.WriteFile(loose, []byte("{0}{Niveau}\n"), 0600)
	if _, err := detectEdition(root, c); err == nil {
		t.Fatal("ignored loose localization")
	}
	os.Remove(loose)
	for _, name := range []string{"Arcanum5.dat", "ArcanumXCustom.dat", "modules/arcanum.patch1", "modules/arcanum.patch9"} {
		p := filepath.Join(root, name)
		if err := writeDAT(p, map[string][]byte{"scr/changed.scr": []byte("mod")}); err != nil {
			t.Fatal(err)
		}
		if _, err := detectEdition(root, c); err == nil {
			t.Fatal("accepted existing patch", name)
		}
		os.Remove(p)
	}
	p := filepath.Join(root, "modules", "arcanum.patch9")
	writeDAT(p, map[string][]byte{"townmap/test.art": []byte("map")})
	if lang, err := detectEdition(root, c); err != nil || lang != "en" {
		t.Fatalf("map-only patch: %s %v", lang, err)
	}
	os.WriteFile(filepath.Join(root, "arcanum1.dat"), []byte("changed art"), 0600)
	if _, err := detectEdition(root, c); err == nil {
		t.Fatal("accepted modified shared archive")
	}
}
func TestOverlayMarkers(t *testing.T) {
	root := t.TempDir()
	for _, lang := range []string{"fr", "en"} {
		path := filepath.Join(root, "modern-ui", "uap-"+lang, "active", "ready")
		os.MkdirAll(filepath.Dir(path), 0700)
		for _, marker := range []string{"", "partial", overlayVersion(lang), "2.0.2-unknown-1\n"} {
			os.WriteFile(path, []byte(marker), 0600)
			if overlayReady(root, lang) {
				t.Fatal("accepted incomplete marker", marker)
			}
		}
		os.WriteFile(path, []byte(overlayVersion(lang)+"\n"), 0600)
		if !overlayReady(root, lang) {
			t.Fatal("rejected completed overlay", lang)
		}
	}
}
