package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestFindCaseSplitDirectories(t *testing.T) {
	root := t.TempDir()
	for _, name := range []string{"Modules/Arcanum.PATCH0", "modules/Arcanum.PATCH1", "modules/Arcanum.PATCH9"} {
		p := filepath.Join(root, filepath.FromSlash(name))
		if err := os.MkdirAll(filepath.Dir(p), 0700); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, []byte(name), 0600); err != nil {
			t.Fatal(err)
		}
	}
	for _, name := range []string{"modules/arcanum.patch0", "MODULES/ARCANUM.PATCH1", `Modules\Arcanum.patch9`} {
		p, err := findCase(root, name)
		if err != nil {
			t.Fatal(name, err)
		}
		data, err := os.ReadFile(p)
		if err != nil || canonical(string(data)) != canonical(name) {
			t.Fatalf("%s: wrong file %q (%v)", name, data, err)
		}
	}
	if _, err := findCase(root, "modules/missing.patch"); !os.IsNotExist(err) {
		t.Fatalf("expected missing file, got %v", err)
	}
}

func TestFindCaseRejectsDuplicateFiles(t *testing.T) {
	root := t.TempDir()
	if err := os.Mkdir(filepath.Join(root, "Modules"), 0700); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(root, "modules")); err == nil {
		t.Skip("requires a case-sensitive filesystem")
	}
	for _, name := range []string{"Modules/Arcanum.PATCH1", "modules/arcanum.patch1"} {
		p := filepath.Join(root, filepath.FromSlash(name))
		if err := os.MkdirAll(filepath.Dir(p), 0700); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, []byte(name), 0600); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := findCase(root, "modules/arcanum.patch1"); err == nil || !strings.Contains(err.Error(), "ambiguous file case") {
		t.Fatalf("expected ambiguous file, got %v", err)
	}
}

func TestFindCaseRejectsInvalidPaths(t *testing.T) {
	for _, name := range []string{"", "/modules", "../modules", "modules/../file", "modules//file", `modules\..\file`} {
		if _, err := findCase(t.TempDir(), name); err == nil {
			t.Fatal("accepted invalid path", name)
		}
	}
}
