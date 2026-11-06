package main

import (
	"os"
	"path/filepath"
	"strings"
)

// UAP installations use Arcanum1.dat; original installations use arcanum1.dat.
// Match the engine's case-insensitive archive lookup on every filesystem.
func hasGameArchive(root string) bool {
	entries, err := os.ReadDir(root)
	if err != nil {
		return false
	}
	for _, entry := range entries {
		if strings.EqualFold(entry.Name(), "arcanum1.dat") {
			info, err := os.Stat(filepath.Join(root, entry.Name()))
			if err == nil && info.Mode().IsRegular() {
				return true
			}
		}
	}
	return false
}
