package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestFeedbackPayloadAndValidation(t *testing.T) {
	calls := 0
	server := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		calls++
		if r.URL.Path != "/feedback" || r.Method != "POST" {
			t.Errorf("unexpected request: %s %s", r.Method, r.URL.Path)
		}
		var payload map[string]string
		if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
			t.Error(err)
		}
		if len(payload) != 2 || payload["message"] != "Un retour de test." || payload["version"] != buildVersion {
			t.Errorf("unexpected payload: %v", payload)
		}
		w.WriteHeader(http.StatusAccepted)
	}))
	defer server.Close()
	oldURL, oldTransport := communityURL, http.DefaultTransport
	communityURL, http.DefaultTransport = server.URL, server.Client().Transport
	defer func() { communityURL, http.DefaultTransport = oldURL, oldTransport }()
	for _, invalid := range []string{"", "court", strings.Repeat("é", 2001), string([]byte{0xff, 0xfe})} {
		if sendFeedback(invalid) == nil {
			t.Error("invalid feedback accepted")
		}
	}
	if calls != 0 {
		t.Fatal("invalid feedback was sent")
	}
	if err := sendFeedback("  Un retour de test.  "); err != nil {
		t.Fatal(err)
	}
	if calls != 1 {
		t.Fatal(calls)
	}
}

func TestFeedbackRejectsRedirectAndServerErrors(t *testing.T) {
	status := http.StatusTooManyRequests
	server := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Location", "https://example.com/private")
		w.WriteHeader(status)
		_, _ = w.Write([]byte("private server diagnostic"))
	}))
	defer server.Close()
	oldURL, oldTransport := communityURL, http.DefaultTransport
	communityURL, http.DefaultTransport = server.URL, server.Client().Transport
	defer func() { communityURL, http.DefaultTransport = oldURL, oldTransport }()
	for _, value := range []int{429, 503, 302, 307} {
		status = value
		err := sendFeedback("A feedback message")
		if err == nil || strings.Contains(err.Error(), "private") {
			t.Fatalf("unsafe result: %v", err)
		}
	}
	communityURL = "http://example.com"
	if sendFeedback("A feedback message") == nil {
		t.Fatal("unencrypted endpoint accepted")
	}
	communityURL = ""
	if sendFeedback("A feedback message") == nil {
		t.Fatal("missing endpoint accepted")
	}
}
