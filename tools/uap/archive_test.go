package main

import (
	"archive/zip"
	"bytes"
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

func TestDATRoundTrip(t *testing.T) {
	p := filepath.Join(t.TempDir(), "test.dat")
	files := map[string][]byte{"mes/test.mes": []byte("{1}{texte accentu\xe9}"), "empty": {}}
	if err := writeDAT(p, files); err != nil {
		t.Fatal(err)
	}
	a, err := openDAT(p)
	if err != nil {
		t.Fatal(err)
	}
	raw, err := os.ReadFile(p)
	if err != nil {
		t.Fatal(err)
	}
	if string(raw[len(raw)-12:len(raw)-8]) != " TAD" {
		t.Fatal("missing native DAT signature")
	}
	expected := len("mes/test.mes") + 1 + len("empty") + 1
	if binary.LittleEndian.Uint32(raw[len(raw)-8:]) != uint32(expected) {
		t.Fatal("incorrect native name table size")
	}
	for name, want := range files {
		got, err := a.read(name)
		if err != nil || !bytes.Equal(got, want) {
			t.Fatal(name, err)
		}
	}
	if _, err = a.read("missing"); !os.IsNotExist(err) {
		t.Fatal(err)
	}
}
func TestBackupNeverReplaces(t *testing.T) {
	root := t.TempDir()
	dir := filepath.Join(root, "Modules", "Arcanum", "Save")
	os.MkdirAll(dir, 0700)
	slot := filepath.Join(dir, "Slot0001.tfaf")
	os.WriteFile(slot, []byte("original"), 0600)
	a, err := backupSaves(root)
	if err != nil {
		t.Fatal(err)
	}
	os.WriteFile(slot, []byte("updated"), 0600)
	b, err := backupSaves(root)
	if err != nil {
		t.Fatal(err)
	}
	if a == b {
		t.Fatal("backup overwritten")
	}
	z, err := zip.OpenReader(a)
	if err != nil {
		t.Fatal(err)
	}
	defer z.Close()
	f, err := z.Open("Modules/Arcanum/Save/Slot0001.tfaf")
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	buf := make([]byte, 8)
	f.Read(buf)
	if string(buf) != "original" {
		t.Fatal(string(buf))
	}
}
func TestBackupRejectsLinkedSaves(t *testing.T) {
	root := t.TempDir()
	os.WriteFile(filepath.Join(root, "external"), []byte("save"), 0600)
	dir := filepath.Join(root, "Save")
	os.Mkdir(dir, 0700)
	if err := os.Symlink(filepath.Join(root, "external"), filepath.Join(dir, "slot")); err != nil {
		t.Skip(err)
	}
	if _, err := backupSaves(root); err == nil {
		t.Fatal("linked save accepted")
	}
}
