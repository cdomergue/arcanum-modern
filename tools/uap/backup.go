package main

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type backupEntry struct {
	Size   int64  `json:"size"`
	SHA256 string `json:"sha256"`
}
type backupManifest struct {
	Created string                 `json:"created"`
	Files   map[string]backupEntry `json:"files"`
}

func fileHash(path string) (string, int64, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", 0, err
	}
	defer f.Close()
	h := sha256.New()
	n, err := io.Copy(h, f)
	return hex.EncodeToString(h.Sum(nil)), n, err
}
func saveFiles(game string) ([]string, error) {
	var roots, files []string
	entries, err := os.ReadDir(game)
	if err != nil {
		return nil, err
	}
	for _, e := range entries {
		n := strings.ToLower(e.Name())
		p := filepath.Join(game, e.Name())
		if n == "save" || n == "saves" {
			roots = append(roots, p)
		}
		if n == "modules" {
			modules, err := os.ReadDir(p)
			if err != nil {
				return nil, err
			}
			for _, m := range modules {
				if m.Type()&os.ModeSymlink != 0 {
					return nil, fmt.Errorf("cannot verify saves in linked module: %s", m.Name())
				}
				if !m.IsDir() {
					continue
				}
				sub, err := os.ReadDir(filepath.Join(p, m.Name()))
				if err != nil {
					return nil, err
				}
				for _, s := range sub {
					if strings.EqualFold(s.Name(), "Save") {
						roots = append(roots, filepath.Join(p, m.Name(), s.Name()))
					}
				}
			}
		}
	}
	for _, root := range roots {
		err := filepath.WalkDir(root, func(path string, d os.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if d.Type()&os.ModeSymlink != 0 {
				return fmt.Errorf("cannot back up linked saves: %s", path)
			}
			if !d.IsDir() {
				if !d.Type().IsRegular() {
					return fmt.Errorf("unsupported save entry: %s", path)
				}
				rel, err := filepath.Rel(game, path)
				if err != nil {
					return err
				}
				files = append(files, filepath.ToSlash(rel))
			}
			return nil
		})
		if err != nil {
			return nil, err
		}
	}
	sort.Strings(files)
	return files, nil
}
func backupSaves(game string) (string, error) {
	files, err := saveFiles(game)
	if err != nil {
		return "", err
	}
	base := filepath.Join(game, "save-backups")
	if err = os.MkdirAll(base, 0700); err != nil {
		return "", err
	}
	dir, err := os.MkdirTemp(base, "before-uap-"+time.Now().UTC().Format("20060102T150405Z")+"-")
	if err != nil {
		return "", err
	}
	partial := filepath.Join(dir, "saves.zip.partial")
	f, err := os.OpenFile(partial, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return "", err
	}
	manifest := backupManifest{time.Now().UTC().Format(time.RFC3339), map[string]backupEntry{}}
	z := zip.NewWriter(f)
	for _, name := range files {
		source := filepath.Join(game, filepath.FromSlash(name))
		input, e := os.Open(source)
		if e != nil {
			err = e
			break
		}
		var output io.Writer
		output, e = z.Create(name)
		if e == nil {
			h := sha256.New()
			var n int64
			n, e = io.Copy(io.MultiWriter(output, h), input)
			manifest.Files[name] = backupEntry{n, hex.EncodeToString(h.Sum(nil))}
		}
		ce := input.Close()
		if e == nil {
			e = ce
		}
		if e != nil {
			err = e
			break
		}
	}
	if err == nil {
		metadata, _ := json.MarshalIndent(manifest, "", "  ")
		w, e := z.Create("backup-manifest.json")
		if e == nil {
			_, e = w.Write(metadata)
		}
		err = e
		if err == nil {
			w, e = z.Create("RESTORE.txt")
			if e == nil {
				_, e = io.WriteString(w, "Close Arcanum before restoring. Preserve any newer saves separately. Disable the UAP overlay by renaming modern-ui/uap-fr/active (French) or modern-ui/uap-en/active (English) to a different name, then copy the saved files back to their matching paths under the game folder. Keep this ZIP intact.\n\nFermez Arcanum avant la restauration. Conservez les nouvelles sauvegardes a part. Desactivez la couche UAP en renommant modern-ui/uap-fr/active (francais) ou modern-ui/uap-en/active (anglais), puis replacez les fichiers sauvegardes aux memes chemins dans le dossier du jeu. Conservez ce ZIP intact.\n")
			}
			err = e
		}
	}
	if e := z.Close(); err == nil {
		err = e
	}
	if e := f.Sync(); err == nil {
		err = e
	}
	if e := f.Close(); err == nil {
		err = e
	}
	if err != nil {
		return "", fmt.Errorf("save backup failed; original saves are unchanged: %w", err)
	}
	reader, err := zip.OpenReader(partial)
	if err != nil {
		return "", err
	}
	for _, entry := range reader.File {
		expected, ok := manifest.Files[entry.Name]
		if !ok {
			continue
		}
		input, e := entry.Open()
		if e != nil {
			err = e
			break
		}
		h := sha256.New()
		n, e := io.Copy(h, input)
		input.Close()
		if e != nil || n != expected.Size || hex.EncodeToString(h.Sum(nil)) != expected.SHA256 {
			err = fmt.Errorf("backup verification failed: %s", entry.Name)
			break
		}
		original, size, e := fileHash(filepath.Join(game, filepath.FromSlash(entry.Name)))
		if e != nil || size != expected.Size || original != expected.SHA256 {
			err = fmt.Errorf("save changed during backup: %s", entry.Name)
			break
		}
	}
	reader.Close()
	if err != nil {
		return "", err
	}
	current, err := saveFiles(game)
	if err != nil {
		return "", err
	}
	if strings.Join(files, "\n") != strings.Join(current, "\n") {
		return "", fmt.Errorf("save list changed during backup")
	}
	final := filepath.Join(dir, "saves.zip")
	if err = os.Rename(partial, final); err != nil {
		return "", err
	}
	return final, nil
}
