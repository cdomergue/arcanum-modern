package main

import (
	"bytes"
	"compress/gzip"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

const hotfixVersion = "1.1.0"
const hotfixRelease = "https://github.com/Dergash/arcanum-uap-hotfixes/releases/download/1.1.0/"

type hotfixCatalog struct {
	Version string                 `json:"version"`
	Assets  map[string]backupEntry `json:"assets"`
	Inputs  map[string][]string    `json:"inputs"`
}
type hotfixLayer struct {
	archive   *datArchive
	directory string
}
type hotfixBase struct {
	language, overlay, destination string
	layers                         []hotfixLayer
}

func loadHotfixCatalog() (hotfixCatalog, error) {
	var c hotfixCatalog
	raw, err := assets.ReadFile("data/hotfix-catalog.json")
	if err == nil {
		err = json.Unmarshal(raw, &c)
	}
	return c, err
}
func hotfixMarker(lang string) string { return hotfixVersion + "-" + lang + "-1\n" }
func hotfixWork(game, lang string) string {
	return filepath.Join(game, "modern-ui", "uap-hotfix", lang)
}
func (b *hotfixBase) read(name string) ([]byte, error) {
	for i := len(b.layers) - 1; i >= 0; i-- {
		layer := b.layers[i]
		if layer.archive != nil {
			if _, ok := layer.archive.entries[canonical(name)]; ok {
				return layer.archive.read(name)
			}
		} else if paths, err := findCases(layer.directory, name); err == nil {
			raw, err := os.ReadFile(paths[0])
			if err != nil {
				return nil, err
			}
			for _, p := range paths[1:] {
				other, err := os.ReadFile(p)
				if err != nil {
					return nil, err
				}
				if !strings.HasPrefix(name, "proto/") || hotfixResourceHash(raw, name) != hotfixResourceHash(other, name) {
					return nil, fmt.Errorf("ambiguous file case: %s", name)
				}
			}
			return raw, nil
		} else if !os.IsNotExist(err) {
			return nil, err
		}
	}
	return nil, os.ErrNotExist
}
func (b *hotfixBase) archive(path string) error {
	a, err := openDAT(path)
	if err == nil {
		b.layers = append(b.layers, hotfixLayer{archive: a})
	}
	return err
}
func (b *hotfixBase) optional(root, name string) error {
	p, err := findCase(root, name)
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return err
	}
	return b.archive(p)
}

// Reproduce the engine's repository order, including loose data. A prepared
// stage is accepted only by the installer, before its atomic activation.
func inspectHotfixBase(game, stage, lang string, c catalog) (*hotfixBase, error) {
	b := &hotfixBase{language: lang, overlay: stage}
	if stage == "" {
		if p, err := findCase(game, "Arcanum5.dat"); err == nil {
			if err = verify(p, c.Patch["arcanum5.dat"]); err != nil {
				return nil, err
			}
			b.language = "en"
			french := true
			for name, want := range c.Original {
				p, e := findCase(game, name)
				if e != nil || verify(p, want) != nil {
					french = false
					break
				}
			}
			if french {
				b.language = "fr"
			}
		} else if !os.IsNotExist(err) {
			return nil, err
		} else {
			for _, l := range []string{"fr", "en"} {
				if overlayReady(game, l) {
					b.overlay = filepath.Join(game, "modern-ui", "uap-"+l, "active")
					b.language = l
					break
				}
			}
			if b.overlay == "" {
				return nil, fmt.Errorf("recognized UAP 2.0.2 is required")
			}
		}
	}
	if b.language != "fr" && b.language != "en" {
		return nil, fmt.Errorf("unknown UAP language")
	}
	b.destination = filepath.Join(hotfixWork(game, b.language), "active")
	if b.overlay != "" {
		b.destination = filepath.Join(b.overlay, "hotfix")
	}
	roots, err := archiveSet(game)
	if err != nil {
		return nil, err
	}
	for _, a := range roots {
		b.layers = append(b.layers, hotfixLayer{archive: a})
	}
	b.layers = append(b.layers, hotfixLayer{directory: filepath.Join(game, "data")})
	if b.overlay != "" {
		for _, name := range []string{"arcanum5.dat", "arcanumxraces.dat"} {
			p, err := findCase(b.overlay, "global/"+name)
			if err != nil {
				return nil, err
			}
			if err = verify(p, c.Patch[name]); err != nil {
				return nil, err
			}
		}
		roots, err = archiveSet(filepath.Join(b.overlay, "global"))
		if err != nil {
			return nil, err
		}
		for _, a := range roots {
			b.layers = append(b.layers, hotfixLayer{archive: a})
		}
		if b.language == "fr" {
			if err = b.archive(filepath.Join(b.overlay, "french-global.dat")); err != nil {
				return nil, err
			}
		}
		b.layers = append(b.layers, hotfixLayer{directory: filepath.Join(b.overlay, "data")})
	}
	if err = b.optional(game, "modules/arcanum.dat"); err != nil {
		return nil, err
	}
	for i := 0; i < 10; i++ {
		if err = b.optional(game, fmt.Sprintf("modules/arcanum.patch%d", i)); err != nil {
			return nil, err
		}
	}
	moduleRoot := game
	if b.overlay != "" {
		moduleRoot = b.overlay
	}
	p, err := findCase(moduleRoot, "modules/arcanum.patch1")
	if err != nil {
		return nil, err
	}
	if err = verify(p, c.Patch["modules/arcanum.patch1"]); err != nil {
		return nil, err
	}
	if b.overlay != "" {
		for _, name := range []string{"modules/arcanum.patch1", "modules/arcanum.patch9"} {
			if err = b.optional(b.overlay, name); err != nil {
				return nil, err
			}
		}
		if b.language == "fr" {
			if err = b.archive(filepath.Join(b.overlay, "french-module.dat")); err != nil {
				return nil, err
			}
		}
	}
	moduleDir, err := findCase(game, "modules/arcanum")
	if err == nil {
		b.layers = append(b.layers, hotfixLayer{directory: moduleDir})
	} else if !os.IsNotExist(err) {
		return nil, err
	}
	return b, nil
}

// These two original prototype layouts are fixed. ObjectID padding and the
// SizeableArray allocator IDs are process-local; all field values and bitsets
// remain part of the fingerprint. Unknown layouts are compared byte-for-byte.
func canonicalHotfixPrototype(raw []byte, name string) []byte {
	size := 0
	var ids []int
	switch canonical(name) {
	case "proto/008219 - armor.pro":
		size, ids = 534, []int{98, 204, 274, 365}
	case "proto/010114 - food.pro":
		size, ids = 464, []int{98, 151, 204, 274}
	}
	if len(raw) != size || size == 0 || binary.LittleEndian.Uint32(raw) != 119 ||
		binary.LittleEndian.Uint16(raw[4:]) != 65535 || binary.LittleEndian.Uint16(raw[28:]) != 1 {
		return raw
	}
	out := append([]byte(nil), raw...)
	clear(out[6:28])
	clear(out[30:36])
	clear(out[40:52])
	for _, id := range ids {
		clear(out[id : id+4])
	}
	return out
}
func hotfixResourceHash(raw []byte, name string) string {
	raw = canonicalHotfixPrototype(raw, name)
	if strings.HasSuffix(name, ".dlg") || strings.HasSuffix(name, ".mes") {
		raw = bytes.ReplaceAll(raw, []byte("\r\n"), []byte("\n"))
	}
	sum := sha256.Sum256(raw)
	return hex.EncodeToString(sum[:])
}
func checkHotfixInputs(b *hotfixBase, c hotfixCatalog) error {
	names := make([]string, 0, len(c.Inputs))
	for name := range c.Inputs {
		names = append(names, name)
	}
	sort.Strings(names)
	for _, name := range names {
		raw, err := b.read(name)
		sum := ""
		if err == nil {
			sum = hotfixResourceHash(raw, name)
		} else if !os.IsNotExist(err) {
			return err
		}
		match := false
		for _, allowed := range c.Inputs[name] {
			if sum == allowed {
				match = true
				break
			}
		}
		if !match {
			return fmt.Errorf("hotfix conflict: %s differs from the supported UAP data", name)
		}
	}
	return nil
}
func hotfixReady(b *hotfixBase, c hotfixCatalog) bool {
	marker, err := os.ReadFile(filepath.Join(b.destination, "ready"))
	if err != nil || string(marker) != hotfixMarker(b.language) {
		return false
	}
	for name, want := range c.Assets {
		if verify(filepath.Join(b.destination, name), want) != nil {
			return false
		}
	}
	raw, err := os.ReadFile(filepath.Join(b.destination, "manifest.json"))
	if err != nil {
		return false
	}
	var manifest struct {
		Files map[string]backupEntry `json:"files"`
	}
	if json.Unmarshal(raw, &manifest) != nil {
		return false
	}
	if b.language == "fr" {
		want, ok := manifest.Files["french-module.dat"]
		if !ok || verify(filepath.Join(b.destination, "french-module.dat"), want) != nil {
			return false
		}
	}
	return true
}
func fetchHotfix(path, url string, want backupEntry) error {
	client := &http.Client{Timeout: 2 * time.Minute, CheckRedirect: func(req *http.Request, via []*http.Request) error {
		if req.URL.Scheme != "https" || len(via) > 5 {
			return fmt.Errorf("invalid hotfix download redirect")
		}
		return nil
	}}
	response, err := client.Get(url)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return fmt.Errorf("hotfix download: HTTP %d", response.StatusCode)
	}
	f, err := os.OpenFile(path, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	_, err = io.Copy(f, io.LimitReader(response.Body, want.Size+1))
	if e := f.Sync(); err == nil {
		err = e
	}
	if e := f.Close(); err == nil {
		err = e
	}
	if err != nil {
		return err
	}
	return verify(path, want)
}
func buildHotfix(game string, b *hotfixBase, output, downloads string, c hotfixCatalog) error {
	if err := checkHotfixInputs(b, c); err != nil {
		return err
	}
	if err := os.MkdirAll(output, 0700); err != nil {
		return err
	}
	progress("hotfix", 0)
	for _, name := range []string{"Arcanum8.dat", "Arcanum.PATCH8"} {
		target := filepath.Join(output, name)
		want := c.Assets[name]
		var err error
		if downloads != "" {
			err = copyVerified(filepath.Join(downloads, name), target, want)
		} else {
			err = fetchHotfix(target, hotfixRelease+name, want)
		}
		if err != nil {
			return err
		}
	}
	if b.language == "fr" {
		progress("translate", 0)
		packed, err := assets.ReadFile("data/hotfix-plan.json.gz")
		if err != nil {
			return err
		}
		reader, err := gzip.NewReader(bytes.NewReader(packed))
		if err != nil {
			return err
		}
		raw, err := io.ReadAll(reader)
		reader.Close()
		if err != nil {
			return err
		}
		var plan translationPlan
		if err = json.Unmarshal(raw, &plan); err != nil {
			return err
		}
		module, err := openDAT(filepath.Join(output, "Arcanum.PATCH8"))
		if err != nil {
			return err
		}
		// Reference-only translations read the original French archives, never the
		// freshly installed English hotfix or a third-party translation.
		var originals []*datArchive
		baseCatalog, err := loadCatalog()
		if err != nil {
			return err
		}
		names := make([]string, 0, len(baseCatalog.Original))
		for name := range baseCatalog.Original {
			names = append(names, name)
		}
		sort.Strings(names)
		for _, name := range names {
			p, e := findCase(game, name)
			if e != nil {
				return e
			}
			a, e := openDAT(p)
			if e != nil {
				return e
			}
			originals = append(originals, a)
		}
		localized := map[string][]byte{}
		for _, file := range plan.Files {
			raw, err := module.read(file.Path)
			if err != nil {
				return err
			}
			text, err := localize(raw, file, func(name string) ([]byte, error) { return readResource(originals, name) })
			if err != nil {
				return err
			}
			localized[file.Path] = text
		}
		if err = writeDAT(filepath.Join(output, "french-module.dat"), localized); err != nil {
			return err
		}
	}
	return nil
}
func finishHotfix(output, lang, backup string) error {
	files := map[string]backupEntry{}
	for _, name := range []string{"Arcanum8.dat", "Arcanum.PATCH8", "french-module.dat"} {
		sum, n, err := fileHash(filepath.Join(output, name))
		if os.IsNotExist(err) && lang == "en" && name == "french-module.dat" {
			continue
		}
		if err != nil {
			return err
		}
		files[name] = backupEntry{n, sum}
	}
	metadata, err := json.MarshalIndent(struct {
		Version, Language, Base, Backup string
		Files                           map[string]backupEntry `json:"files"`
	}{hotfixVersion, lang, "UAP 2.0.2", backup, files}, "", "  ")
	if err != nil {
		return err
	}
	if err = os.WriteFile(filepath.Join(output, "manifest.json"), metadata, 0600); err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(output, "ready"), []byte(hotfixMarker(lang)), 0600)
}
func installHotfix(game, downloads string, c catalog) error {
	b, err := inspectHotfixBase(game, "", "", c)
	if err != nil {
		return err
	}
	h, err := loadHotfixCatalog()
	if err != nil {
		return err
	}
	if hotfixReady(b, h) {
		return nil
	}
	if _, err = os.Stat(b.destination); !os.IsNotExist(err) {
		return fmt.Errorf("existing hotfix directory is not recognized; keep it for inspection: %s", b.destination)
	}
	if err = os.MkdirAll(filepath.Dir(b.destination), 0700); err != nil {
		return err
	}
	stage, err := os.MkdirTemp(filepath.Dir(b.destination), "hotfix-stage-")
	if err != nil {
		return err
	}
	defer os.RemoveAll(stage)
	if err = buildHotfix(game, b, stage, downloads, h); err != nil {
		return err
	}
	progress("backup", 0)
	relative, err := filepath.Rel(game, b.destination)
	if err != nil {
		return err
	}
	backup, err := backupSavesWithInstructions(game, fmt.Sprintf(
		"Close Arcanum. Keep newer saves separately. Rename only %s to disable the hotfix, keeping UAP installed. Restore saved files to their matching paths under the game folder. Keep this ZIP intact.\n\nFermez Arcanum. Conservez les nouvelles sauvegardes a part. Renommez uniquement %s pour desactiver le hotfix en conservant l'UAP. Replacez les fichiers sauvegardes aux memes chemins dans le dossier du jeu. Conservez ce ZIP intact.\n", relative, relative))
	if err != nil {
		return err
	}
	if err = finishHotfix(stage, b.language, backup); err != nil {
		return err
	}
	if err = os.Rename(stage, b.destination); err != nil {
		return err
	}
	progress("ready", 100)
	return nil
}
func probeHotfix(game string, c catalog) int {
	b, err := inspectHotfixBase(game, "", "", c)
	if err != nil {
		fmt.Println("Hotfix not offered:", err)
		return 0
	}
	h, err := loadHotfixCatalog()
	if err != nil {
		return 0
	}
	if hotfixReady(b, h) {
		return 0
	}
	if _, err = os.Stat(b.destination); !os.IsNotExist(err) {
		fmt.Println("Unrecognized existing hotfix; leaving it unchanged")
		return 0
	}
	choice, err := os.ReadFile(filepath.Join(hotfixWork(game, b.language), "choice"))
	if err == nil && string(choice) == "no "+hotfixVersion+"\n" {
		return 0
	}
	if err = checkHotfixInputs(b, h); err != nil {
		fmt.Println("Hotfix not offered:", err)
		return 0
	}
	if b.language == "fr" {
		return 22
	}
	return 23
}
