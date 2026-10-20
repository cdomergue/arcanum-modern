package main

import (
	"bytes"
	"compress/zlib"
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type datEntry struct{ flags, size, packed, offset uint32 }
type datArchive struct {
	path    string
	entries map[string]datEntry
}

func canonical(s string) string { return strings.ToLower(strings.ReplaceAll(s, `\`, `/`)) }
func openDAT(path string) (*datArchive, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	st, err := f.Stat()
	if err != nil {
		return nil, err
	}
	if st.Size() < 12 {
		return nil, fmt.Errorf("invalid DAT: %s", path)
	}
	var footer [4]byte
	if _, err = f.ReadAt(footer[:], st.Size()-4); err != nil {
		return nil, err
	}
	offset := int64(binary.LittleEndian.Uint32(footer[:]))
	if offset < 8 || offset > st.Size()-4 {
		return nil, fmt.Errorf("invalid DAT index: %s", path)
	}
	table := make([]byte, offset)
	if _, err = f.ReadAt(table, st.Size()-4-offset); err != nil {
		return nil, err
	}
	tableSize := int64(binary.LittleEndian.Uint32(table))
	count := binary.LittleEndian.Uint32(table[4:])
	base := st.Size() - tableSize - offset
	if base < 0 || count > 1000000 {
		return nil, fmt.Errorf("invalid DAT table: %s", path)
	}
	a := &datArchive{path: path, entries: map[string]datEntry{}}
	pos := 8
	for i := uint32(0); i < count; i++ {
		if pos+4 > len(table) {
			return nil, io.ErrUnexpectedEOF
		}
		n := int(binary.LittleEndian.Uint32(table[pos:]))
		pos += 4
		if n < 1 || n > 4096 || pos+n+20 > len(table) {
			return nil, fmt.Errorf("invalid DAT entry: %s", path)
		}
		name := canonical(strings.TrimRight(string(table[pos:pos+n]), "\x00"))
		pos += n
		flags := binary.LittleEndian.Uint32(table[pos+4:])
		size := binary.LittleEndian.Uint32(table[pos+8:])
		packed := binary.LittleEndian.Uint32(table[pos+12:])
		at := base + int64(binary.LittleEndian.Uint32(table[pos+16:]))
		pos += 20
		if flags&(0x400|0x800) != 0 {
			continue
		}
		stored := size
		if flags&2 != 0 {
			stored = packed
		}
		if at < 0 || at+int64(stored) > st.Size() || size > 256*1024*1024 || at > int64(^uint32(0)) {
			return nil, fmt.Errorf("invalid DAT extent: %s", name)
		}
		a.entries[name] = datEntry{flags, size, packed, uint32(at)}
	}
	return a, nil
}
func (a *datArchive) read(name string) ([]byte, error) {
	e, ok := a.entries[canonical(name)]
	if !ok {
		return nil, os.ErrNotExist
	}
	f, err := os.Open(a.path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	n := e.size
	if e.flags&2 != 0 {
		n = e.packed
	}
	raw := make([]byte, n)
	if _, err = f.ReadAt(raw, int64(e.offset)); err != nil {
		return nil, err
	}
	if e.flags&2 == 0 {
		return raw, nil
	}
	r, err := zlib.NewReader(bytes.NewReader(raw))
	if err != nil {
		return nil, err
	}
	defer r.Close()
	data, err := io.ReadAll(io.LimitReader(r, int64(e.size)+1))
	if err != nil {
		return nil, err
	}
	if len(data) != int(e.size) {
		return nil, fmt.Errorf("DAT length mismatch: %s", name)
	}
	return data, nil
}
func writeDAT(path string, files map[string][]byte) error {
	var body, index bytes.Buffer
	nameBytes := 0
	names := make([]string, 0, len(files))
	for name := range files {
		names = append(names, name)
	}
	sort.Strings(names)
	put := func(w io.Writer, n uint32) { binary.Write(w, binary.LittleEndian, n) }
	for _, name := range names {
		raw := files[name]
		var compressed bytes.Buffer
		z := zlib.NewWriter(&compressed)
		z.Write(raw)
		z.Close()
		at := body.Len()
		body.Write(compressed.Bytes())
		name = strings.ReplaceAll(name, "/", `\`) + "\x00"
		nameBytes += len(name)
		put(&index, uint32(len(name)))
		index.WriteString(name)
		put(&index, 0)
		put(&index, 2)
		put(&index, uint32(len(raw)))
		put(&index, uint32(compressed.Len()))
		put(&index, uint32(at))
	}
	// DAT readers calculate data offsets from the footer and index sizes.
	tableSize := index.Len() + 16
	put(&body, uint32(body.Len()+4))
	put(&body, uint32(len(names)))
	body.Write(index.Bytes())
	body.WriteString(" TAD")
	put(&body, uint32(nameBytes))
	put(&body, uint32(tableSize))
	return os.WriteFile(path, body.Bytes(), 0600)
}
func archiveSet(root string) ([]*datArchive, error) {
	var paths []string
	entries, err := os.ReadDir(root)
	if err != nil {
		return nil, err
	}
	for _, p := range entries {
		n := strings.ToLower(p.Name())
		if !p.IsDir() && strings.HasPrefix(n, "arcanum") && strings.HasSuffix(n, ".dat") {
			paths = append(paths, filepath.Join(root, p.Name()))
		}
	}
	sort.Slice(paths, func(i, j int) bool { return strings.ToLower(paths[i]) < strings.ToLower(paths[j]) })
	var result []*datArchive
	for _, p := range paths {
		a, err := openDAT(p)
		if err != nil {
			return nil, err
		}
		result = append(result, a)
	}
	return result, nil
}
func readResource(repos []*datArchive, name string) ([]byte, error) {
	for i := len(repos) - 1; i >= 0; i-- {
		if _, ok := repos[i].entries[canonical(name)]; ok {
			return repos[i].read(name)
		}
	}
	return nil, fmt.Errorf("missing original French resource: %s", name)
}
