package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"sort"
	"strings"
	"time"
)

const releasesURL = "https://github.com/cdomergue/arcanum-modern/releases"
const historyAPI = "https://api.github.com/repos/cdomergue/arcanum-modern/releases"

// Notes are independent of the authoritative latest-release update manifest.
func recentReleases(items []release) []release {
	seen := map[string]bool{}
	var result []release
	for _, r := range items {
		v, valid := version(r.Tag)
		key := fmt.Sprint(v)
		if !valid || r.Draft || r.Prerelease || r.Published == "" || seen[key] {
			continue
		}
		seen[key] = true
		r.Assets = nil
		result = append(result, r)
	}
	sort.SliceStable(result, func(i, j int) bool { return newer(result[j].Tag, result[i].Tag) })
	if len(result) > 5 {
		result = result[:5]
	}
	return result
}

func releaseHistory(ctx context.Context, client *http.Client, endpoint string) ([]release, error) {
	ctx, cancel := context.WithTimeout(ctx, 6*time.Second)
	defer cancel()
	var result []release
	for page := 1; ; page++ {
		b, err := readRemote(ctx, client, fmt.Sprintf("%s?per_page=30&page=%d", endpoint, page), 4<<20)
		if err != nil {
			return nil, err
		}
		var batch []release
		if err := json.Unmarshal(b, &batch); err != nil {
			return nil, err
		}
		result = recentReleases(append(result, batch...))
		if len(result) == 5 || len(batch) < 30 {
			return result, nil
		}
	}
}

func historyText(items []release) string {
	var sections []string
	for _, r := range recentReleases(items) {
		sections = append(sections, r.Tag+"\r\n"+strings.Repeat("—", 24)+"\r\n"+releaseText(r.Body))
	}
	return strings.Join(sections, "\r\n\r\n")
}
