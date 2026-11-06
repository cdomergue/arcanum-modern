package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestGameArchiveNames(t *testing.T) {
	for _, name := range []string{"arcanum1.dat", "Arcanum1.dat", "ARCANUM1.DAT", "aRcAnUm1.DaT"} {
		t.Run(name, func(t *testing.T) {
			root := t.TempDir()
			if hasGameArchive(root) {
				t.Fatal("accepted an empty directory")
			}
			put(t, root, name, []byte("archive fixture"))
			if !hasGameArchive(root) {
				t.Fatalf("rejected %s", name)
			}
		})
	}
}

func TestGameArchiveMustBeAFile(t *testing.T) {
	root := t.TempDir()
	put(t, root, "arcanum1.dat.bak", []byte("backup"))
	put(t, root, "arcanum2.dat", []byte("other archive"))
	if err := os.Mkdir(filepath.Join(root, "Arcanum1.dat"), 0755); err != nil {
		t.Fatal(err)
	}
	if hasGameArchive(root) || hasGameArchive(filepath.Join(root, "missing")) {
		t.Fatal("accepted a directory or a missing archive")
	}
}

func TestGameArchiveSymlink(t *testing.T) {
	root := t.TempDir()
	archive := filepath.Join(root, "original.dat")
	if err := os.Symlink(archive, filepath.Join(root, "Arcanum1.dat")); err != nil {
		t.Skipf("symlinks unavailable: %v", err)
	}
	if hasGameArchive(root) {
		t.Fatal("accepted a dangling symlink")
	}
	put(t, root, "original.dat", []byte("archive fixture"))
	if !hasGameArchive(root) {
		t.Fatal("rejected a symlink to an archive")
	}
}
