package main

import (
	"archive/zip"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

func TestDistributedPackage(t *testing.T) {
	archive := os.Getenv("RENAISSANCE_LAUNCHER_PACKAGE")
	if archive == "" {
		t.Skip("set RENAISSANCE_LAUNCHER_PACKAGE to verify the real ZIP")
	}
	z, e := zip.OpenReader(archive)
	if e != nil {
		t.Fatal(e)
	}
	var m manifest
	for _, f := range z.File {
		if f.Name == manifestName {
			r, e := f.Open()
			if e != nil {
				t.Fatal(e)
			}
			e = json.NewDecoder(r).Decode(&m)
			r.Close()
			if e != nil {
				t.Fatal(e)
			}
		}
	}
	z.Close()
	if _, e = unpack(archive, filepath.Join(t.TempDir(), "payload"), m.Version); e != nil {
		t.Fatal(e)
	}
}
func TestDownloadWorkspaceRecovery(t *testing.T) {
	root := t.TempDir()
	work, e := downloadWorkspace(root)
	if e != nil {
		t.Fatal(e)
	}
	put(t, work, "update.zip", []byte("interrupted"))
	again, e := downloadWorkspace(root)
	if e != nil || again != work || exists(filepath.Join(work, "update.zip")) {
		t.Fatal("staging not recovered", e)
	}
	_ = os.WriteFile(filepath.Join(work, "owner"), []byte("user content"), 0600)
	if _, e = downloadWorkspace(root); e == nil {
		t.Fatal("unrecognized directory removed")
	}
}
func TestJournalDirectoryInterruption(t *testing.T) {
	root := t.TempDir()
	fixture(t, root, "2.0.1")
	if e := writeJSON(filepath.Join(root, ".renaissance-update-journal"), journal{Format: 1}); e != nil {
		t.Fatal(e)
	}
	if e := os.Mkdir(filepath.Join(root, transactionDir), 0700); e != nil {
		t.Fatal(e)
	}
	if e := recoverInstall(root); e != nil {
		t.Fatal(e)
	}
	if exists(filepath.Join(root, transactionDir)) {
		t.Fatal("interrupted journal not recovered")
	}
}
