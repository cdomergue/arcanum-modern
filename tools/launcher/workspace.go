package main

import (
	"fmt"
	"os"
	"path/filepath"
)

// Reuse one marked staging directory. A killed download must not accumulate
// entire release archives on every subsequent attempt.
func downloadWorkspace(root string) (string, error) {
	name := filepath.Join(root, ".renaissance-download")
	if info, err := os.Lstat(name); err == nil {
		marker, e := os.ReadFile(filepath.Join(name, "owner"))
		if !info.IsDir() || info.Mode()&os.ModeSymlink != 0 || e != nil || string(marker) != "Arcanum Renaissance download v1\n" {
			return "", fmt.Errorf("unrecognized staging directory")
		}
		if e = os.RemoveAll(name); e != nil {
			return "", e
		}
	} else if !os.IsNotExist(err) {
		return "", err
	}
	if err := os.Mkdir(name, 0700); err != nil {
		return "", err
	}
	if err := os.WriteFile(filepath.Join(name, "owner"), []byte("Arcanum Renaissance download v1\n"), 0600); err != nil {
		_ = os.Remove(name)
		return "", err
	}
	return name, nil
}
