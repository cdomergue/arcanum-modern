package main

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path"
	"path/filepath"
	"sort"
	"strings"
)

type manifest struct {
	Version          string            `json:"version"`
	Format           int               `json:"launcher_format"`
	Target           string            `json:"target"`
	IncludesGameData bool              `json:"includes_game_data"`
	Hashes           map[string]string `json:"sha256"`
}

// Only distributed mod resources can be updated. The game, saves, generated
// content, UAP installation, preferences and custom creations are outside here.
func allowed(name string) bool {
	if len(name) > 220 || strings.ContainsAny(name, "\\:$\x00") || path.Clean(name) != name || strings.HasPrefix(name, "/") {
		return false
	}
	for _, part := range strings.Split(name, "/") {
		if part == "" || strings.HasPrefix(part, ".") || strings.HasSuffix(part, ".") || strings.HasSuffix(part, " ") {
			return false
		}
		upper := strings.ToUpper(strings.SplitN(part, ".", 2)[0])
		if upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" || strings.HasPrefix(upper, "COM") && len(upper) == 4 || strings.HasPrefix(upper, "LPT") && len(upper) == 4 {
			return false
		}
		switch strings.ToLower(part) {
		case "save", "players", "screenshots", "portraits", "backgrounds", "local-maps", "active":
			return false
		}
	}
	switch name {
	case "arcanum-modern", "renaissance-launcher", "Play Arcanum Renaissance.sh", "Play Arcanum Renaissance.command",
		"ArcanumModern/launcher/renaissance-launcher-ui", "ArcanumModern/launcher/background.png",
		"ArcanumModern/uap/uap-helper", "ArcanumModern/uap/7zz",
		"ArcanumModern/lib/libavformat.so.62", "ArcanumModern/lib/libavcodec.so.62", "ArcanumModern/lib/libavutil.so.60",
		"Arcanum Modern.app/Contents/Info.plist", "Arcanum Modern.app/Contents/PkgInfo",
		"Arcanum Modern.app/Contents/MacOS/Arcanum Community Edition", "Arcanum Modern.app/Contents/MacOS/renaissance-launcher",
		"Arcanum Modern.app/Contents/_CodeSignature/CodeResources",
		"Arcanum Modern.app/Contents/Frameworks/libavformat.62.dylib", "Arcanum Modern.app/Contents/Frameworks/libavcodec.62.dylib", "Arcanum Modern.app/Contents/Frameworks/libavutil.60.dylib":
		return true
	case "arcanum-modern.exe", "renaissance-launcher.exe", "Play Arcanum Renaissance.bat", "avformat-62.dll", "avcodec-62.dll", "avutil-60.dll", manifestName,
		"ArcanumModern/README.md", "ArcanumModern/INSTALL.txt", "ArcanumModern/uap/uap-helper.exe", "ArcanumModern/uap/7z.exe", "ArcanumModern/uap/7z.dll",
		"ArcanumModern/movies/train_leaving.bik", "ArcanumModern/world-updates/bundled.mes":
		return true
	}
	if strings.HasPrefix(name, "modern-ui/") {
		asset := strings.TrimPrefix(name, "modern-ui/")
		for _, item := range []string{"Book.ttf", "Heading.ttf", "FONT-LICENSE.txt", "menu-background.png", "main-menu.png", "single-player.png", "death-background.png", "inventory-actions.png", "skill-icons.png", "hud-atlas.png", "top-bar.png", "bottom-bar.png", "play-panel.png", "control-icons.png", "splash/splash1.png", "splash/splash2.png", "splash/splash3.png"} {
			if asset == item {
				return true
			}
		}
		return false
	}
	for _, prefix := range []string{"Arcanum Modern.app/Contents/Resources/", "ArcanumModern/licenses/", "ArcanumModern/docs/", "ArcanumModern/house/base/", "ArcanumModern/house/en/", "ArcanumModern/house/fr/", "ArcanumModern/tutorial/base/", "ArcanumModern/tutorial/en/", "ArcanumModern/tutorial/fr/", "ArcanumModern/world-updates/housing."} {
		if strings.HasPrefix(name, prefix) {
			return true
		}
	}
	return false
}
func fileHash(name string) (string, error) {
	f, e := os.Open(name)
	if e != nil {
		return "", e
	}
	defer f.Close()
	h := sha256.New()
	_, e = io.Copy(h, f)
	return hex.EncodeToString(h.Sum(nil)), e
}
func safePath(root, name string) (string, error) {
	if info, err := os.Lstat(root); err == nil && (!info.IsDir() || info.Mode()&os.ModeSymlink != 0) {
		return "", fmt.Errorf("linked or invalid resource root")
	}
	if !allowed(name) {
		return "", fmt.Errorf("protected path: %s", name)
	}
	current := root
	for _, part := range strings.Split(name, "/") {
		current = filepath.Join(current, part)
		info, err := os.Lstat(current)
		if errors.Is(err, os.ErrNotExist) {
			continue
		}
		if err != nil {
			return "", err
		}
		if info.Mode()&os.ModeSymlink != 0 || (!info.IsDir() && !info.Mode().IsRegular()) {
			return "", fmt.Errorf("linked or special path: %s", name)
		}
	}
	return current, nil
}
func readManifest(root string) (manifest, error) {
	var m manifest
	name, err := safePath(root, manifestName)
	if err != nil {
		return m, err
	}
	b, err := os.ReadFile(name)
	if err == nil {
		err = json.Unmarshal(b, &m)
	}
	return m, err
}
func unpack(archive, destination, tag string) (manifest, error) {
	var m manifest
	z, err := zip.OpenReader(archive)
	if err != nil {
		return m, err
	}
	defer z.Close()
	if len(z.File) > 25000 {
		return m, fmt.Errorf("too many files")
	}
	entries := map[string]*zip.File{}
	folded := map[string]bool{}
	var total uint64
	for _, f := range z.File {
		key := strings.ToLower(f.Name)
		if !allowed(f.Name) || folded[key] || !f.Mode().IsRegular() {
			return m, fmt.Errorf("unsafe or duplicate ZIP entry: %s", f.Name)
		}
		folded[key] = true
		entries[f.Name] = f
		total += f.UncompressedSize64
		if f.UncompressedSize64 > 2<<30 || total > 8<<30 {
			return m, fmt.Errorf("unpacked size limit")
		}
	}
	mf := entries[manifestName]
	if mf == nil || mf.UncompressedSize64 > 4<<20 {
		return m, fmt.Errorf("missing manifest")
	}
	stream, err := mf.Open()
	if err != nil {
		return m, err
	}
	raw, err := io.ReadAll(io.LimitReader(stream, (4<<20)+1))
	stream.Close()
	if err != nil {
		return m, err
	}
	if err = json.Unmarshal(raw, &m); err != nil {
		return m, err
	}
	if m.Format != 1 || m.Target != targetName() || m.IncludesGameData || strings.TrimPrefix(m.Version, "v") != strings.TrimPrefix(tag, "v") || len(m.Hashes) != len(entries)-1 {
		return m, fmt.Errorf("incompatible manifest")
	}
	for _, required := range []string{gameRelative(), launcherRelative(), playRelative()} {
		if _, ok := m.Hashes[required]; !ok {
			return m, fmt.Errorf("missing %s", required)
		}
	}
	for name, digest := range m.Hashes {
		b, e := hex.DecodeString(digest)
		if e != nil || len(b) != 32 || name == manifestName || entries[name] == nil {
			return m, fmt.Errorf("invalid manifest entry: %s", name)
		}
	}
	for _, f := range z.File {
		target, err := safePath(destination, f.Name)
		if err != nil {
			return m, err
		}
		if err = os.MkdirAll(filepath.Dir(target), 0700); err != nil {
			return m, err
		}
		input, err := f.Open()
		if err != nil {
			return m, err
		}
		output, err := os.OpenFile(target, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
		if err != nil {
			input.Close()
			return m, err
		}
		h := sha256.New()
		n, copyErr := io.Copy(io.MultiWriter(output, h), io.LimitReader(input, int64(f.UncompressedSize64)+1))
		input.Close()
		mode := os.FileMode(0644)
		if f.Mode().Perm()&0111 != 0 {
			mode = 0755
		}
		modeErr := output.Chmod(mode)
		syncErr := output.Sync()
		closeErr := output.Close()
		if modeErr != nil {
			return m, modeErr
		}
		if copyErr != nil {
			return m, copyErr
		}
		if syncErr != nil {
			return m, syncErr
		}
		if closeErr != nil {
			return m, closeErr
		}
		if uint64(n) != f.UncompressedSize64 || (f.Name != manifestName && !strings.EqualFold(hex.EncodeToString(h.Sum(nil)), m.Hashes[f.Name])) {
			return m, fmt.Errorf("file checksum mismatch: %s", f.Name)
		}
	}
	return m, nil
}

const transactionDir = ".renaissance-update"

type change struct {
	Name     string
	Original bool
	Remove   bool
}
type journal struct {
	Format  int
	Changes []change
}

func writeJSON(name string, v any) error {
	raw, err := json.Marshal(v)
	if err != nil {
		return err
	}
	f, err := os.OpenFile(name, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	_, err = f.Write(raw)
	if err == nil {
		err = f.Sync()
	}
	closeErr := f.Close()
	if err == nil {
		err = closeErr
	}
	return err
}
func exists(name string) bool { _, e := os.Lstat(name); return e == nil }
func transactionPath(root string) (string, error) {
	dir := filepath.Join(root, transactionDir)
	if info, e := os.Lstat(dir); e == nil {
		if !info.IsDir() || info.Mode()&os.ModeSymlink != 0 {
			return "", fmt.Errorf("invalid update directory")
		}
	} else if !errors.Is(e, os.ErrNotExist) {
		return "", e
	}
	return dir, nil
}
func recoverInstall(root string) error {
	dir, err := transactionPath(root)
	if err != nil {
		return err
	}
	if !exists(dir) {
		return nil
	}
	raw, err := os.ReadFile(filepath.Join(dir, "journal.json"))
	if errors.Is(err, os.ErrNotExist) && exists(filepath.Join(root, ".renaissance-update-journal")) {
		// Interrupted between creating the directory and moving the journal;
		// no installed file has been touched at this point.
		if err = os.Rename(filepath.Join(root, ".renaissance-update-journal"), filepath.Join(dir, "journal.json")); err == nil {
			raw, err = os.ReadFile(filepath.Join(dir, "journal.json"))
		}
	}
	if err != nil {
		return fmt.Errorf("update recovery requires journal: %w", err)
	}
	var j journal
	if json.Unmarshal(raw, &j) != nil || j.Format != 1 || len(j.Changes) > 25000 {
		return fmt.Errorf("invalid update journal")
	}
	seen := map[string]bool{}
	for _, c := range j.Changes {
		if !allowed(c.Name) || seen[strings.ToLower(c.Name)] {
			return fmt.Errorf("invalid journal path")
		}
		seen[strings.ToLower(c.Name)] = true
	}
	committed := exists(filepath.Join(dir, "committed"))
	if !committed {
		for i := len(j.Changes) - 1; i >= 0; i-- {
			c := j.Changes[i]
			target, e := safePath(root, c.Name)
			if e != nil {
				return e
			}
			backup, e := safePath(filepath.Join(dir, "backup"), c.Name)
			if e != nil {
				return e
			}
			if exists(backup) {
				// A missing backup on a second recovery means this entry was restored.
				oldHash, oldErr := fileHash(backup)
				currentHash, currentErr := fileHash(target)
				if oldErr == nil && currentErr == nil && oldHash == currentHash {
					continue
				}
				if e = os.MkdirAll(filepath.Dir(target), 0755); e != nil {
					return e
				}
				if e = os.Rename(backup, target); e != nil {
					return e
				}
			} else if !c.Original && !c.Remove {
				if e = os.Remove(target); e != nil && !errors.Is(e, os.ErrNotExist) {
					return e
				}
			}
		}
	}
	err = os.RemoveAll(dir)
	if committed {
		return nil
	} // Installed files are complete; retry cleanup later.
	return err
}

// Payload is verified before this function runs. Journal and backups make a
// failed or interrupted multi-file replacement recoverable at the next launch.
func install(root, payload string, next manifest, step func(int) error) error {
	if err := recoverInstall(root); err != nil {
		return err
	}
	old, err := readManifest(root)
	if err != nil {
		return fmt.Errorf("installed manifest: %w", err)
	}
	changes := []change{}
	names := []string{}
	for n := range next.Hashes {
		names = append(names, n)
	}
	sort.Strings(names)
	names = append(names, manifestName)
	for _, name := range names {
		target, e := safePath(root, name)
		if e != nil {
			return e
		}
		info, e := os.Lstat(target)
		present := e == nil
		if e != nil && !errors.Is(e, os.ErrNotExist) {
			return e
		}
		if present {
			if !info.Mode().IsRegular() {
				return fmt.Errorf("destination is not a file: %s", name)
			}
			if name != manifestName {
				if _, owned := old.Hashes[name]; !owned {
					return fmt.Errorf("unmanaged file would be replaced: %s", name)
				}
			}
		}
		changes = append(changes, change{Name: name, Original: present})
	}
	// Remove only obsolete files still identical to the old package. Preserve
	// modified files and every unlisted file, including custom content.
	for name, digest := range old.Hashes {
		if _, ok := next.Hashes[name]; ok || !allowed(name) || name == manifestName {
			continue
		}
		target, e := safePath(root, name)
		if e != nil {
			return e
		}
		h, e := fileHash(target)
		if e == nil && strings.EqualFold(h, digest) {
			changes = append(changes, change{Name: name, Original: true, Remove: true})
		}
	}
	dir, err := transactionPath(root)
	if err != nil {
		return err
	}
	// Write a complete journal before creating the transaction directory. Its
	// rename is the point after which recovery is required.
	pending := filepath.Join(root, ".renaissance-update-journal")
	if exists(pending) {
		if err = os.Remove(pending); err != nil {
			return err
		}
	}
	if err = writeJSON(pending, journal{Format: 1, Changes: changes}); err != nil {
		return err
	}
	if err = os.Mkdir(dir, 0700); err != nil {
		return err
	}
	if err = os.Rename(pending, filepath.Join(dir, "journal.json")); err != nil {
		os.Remove(dir)
		return err
	}
	for i, c := range changes {
		err = func() error {
			if step != nil {
				if e := step(i); e != nil {
					return e
				}
			}
			target, e := safePath(root, c.Name)
			if e != nil {
				return e
			}
			if c.Original {
				backup, e := safePath(filepath.Join(dir, "backup"), c.Name)
				if e != nil {
					return e
				}
				if e = os.MkdirAll(filepath.Dir(backup), 0700); e != nil {
					return e
				}
				if e = backupFile(target, backup); e != nil {
					return e
				}
			}
			if c.Remove {
				return os.Remove(target)
			}
			if e = os.MkdirAll(filepath.Dir(target), 0755); e != nil {
				return e
			}
			staged, e := safePath(payload, c.Name)
			if e != nil {
				return e
			}
			return os.Rename(staged, target)
		}()
		if err != nil {
			if rollback := recoverInstall(root); rollback != nil {
				return fmt.Errorf("update: %v; recovery: %w", err, rollback)
			}
			return err
		}
	}
	if err = writeJSON(filepath.Join(dir, "committed"), true); err != nil {
		rollback := recoverInstall(root)
		return fmt.Errorf("commit marker: %v; recovery: %v", err, rollback)
	}
	// Cleanup may be retried next time (for example if a scanner locks a backup).
	_ = recoverInstall(root)
	return nil
}

// Keep the installed executable present until its replacement is ready. This
// is particularly important for the launcher: after an interruption the user's
// shortcut must still have an executable through which recovery can run.
func backupFile(source, backup string) error {
	in, err := os.Open(source)
	if err != nil {
		return err
	}
	defer in.Close()
	partial := backup + ".copying"
	out, err := os.OpenFile(partial, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	info, statErr := in.Stat()
	if statErr != nil {
		out.Close()
		return statErr
	}
	if err = out.Chmod(info.Mode().Perm()); err != nil {
		out.Close()
		return err
	}
	_, err = io.Copy(out, in)
	if err == nil {
		err = out.Sync()
	}
	closeErr := out.Close()
	if err == nil {
		err = closeErr
	}
	if err != nil {
		return err
	}
	return os.Rename(partial, backup)
}
