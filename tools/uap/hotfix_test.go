package main

import (
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

func TestHotfixPrototypeCacheIdentity(t *testing.T) {
	name := "proto/010114 - food.pro"
	original := make([]byte, 464)
	binary.LittleEndian.PutUint32(original, 119)
	binary.LittleEndian.PutUint16(original[4:], 65535)
	binary.LittleEndian.PutUint16(original[28:], 1)
	cached := append([]byte(nil), original...)
	cached[31], cached[99] = 42, 67 // Padding and allocator ID only.
	if hotfixResourceHash(original, name) != hotfixResourceHash(cached, name) {
		t.Fatal("generated cache rejected")
	}
	cached[300] = 1 // A persisted object field must never be ignored.
	if hotfixResourceHash(original, name) == hotfixResourceHash(cached, name) {
		t.Fatal("changed object accepted")
	}
	cached = append(cached, 0)
	if hotfixResourceHash(original, name) == hotfixResourceHash(cached, name) {
		t.Fatal("unknown layout accepted")
	}
}

func TestHotfixLooseConflictsAndCaseVariants(t *testing.T) {
	root := t.TempDir()
	name := "proto/010114 - food.pro"
	raw := make([]byte, 464)
	binary.LittleEndian.PutUint32(raw, 119)
	binary.LittleEndian.PutUint16(raw[4:], 65535)
	binary.LittleEndian.PutUint16(raw[28:], 1)
	for _, dir := range []string{"Proto", "proto"} {
		if err := os.MkdirAll(filepath.Join(root, dir), 0700); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(root, dir, "010114 - food.pro"), raw, 0600); err != nil {
			t.Fatal(err)
		}
	}
	b := &hotfixBase{layers: []hotfixLayer{{directory: root}}}
	c := hotfixCatalog{Inputs: map[string][]string{name: {hotfixResourceHash(raw, name)}}}
	if err := checkHotfixInputs(b, c); err != nil {
		t.Fatal(err)
	}
	raw[300] = 1
	if err := os.WriteFile(filepath.Join(root, name), raw, 0600); err != nil {
		t.Fatal(err)
	}
	if err := checkHotfixInputs(b, c); err == nil {
		t.Fatal("conflicting loose prototype accepted")
	}
}

func TestHotfixMetadataAndCorruptDownload(t *testing.T) {
	c, err := loadHotfixCatalog()
	if err != nil {
		t.Fatal(err)
	}
	if c.Version != hotfixVersion || len(c.Inputs) != 405 || len(c.Assets) != 2 {
		t.Fatal("incomplete pinned catalog")
	}
	download := t.TempDir()
	if err := os.WriteFile(filepath.Join(download, "Arcanum8.dat"), []byte("bad download"), 0600); err != nil {
		t.Fatal(err)
	}
	// Empty inputs isolate integrity checking from compatibility checking.
	c.Inputs = nil
	output := filepath.Join(t.TempDir(), "stage")
	if err := buildHotfix(t.TempDir(), &hotfixBase{language: "en"}, output, download, c); err == nil {
		t.Fatal("corrupt download accepted")
	}
	if _, err := os.Stat(filepath.Join(output, "ready")); !os.IsNotExist(err) {
		t.Fatal("failed preparation activated")
	}
}
