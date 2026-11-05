package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"
)

// A separate process exercises the actual argument, log and progress protocol
// without downloading patches or touching a real installation.
func init() {
	if os.Getenv("RENAISSANCE_TEST_UAP") != "1" || !strings.HasPrefix(filepath.Base(os.Args[0]), "uap-helper") {
		return
	}
	values := map[string]string{}
	for i := 1; i+1 < len(os.Args); i += 2 {
		values[os.Args[i]] = os.Args[i+1]
	}
	root, mode, lang := values["--game"], values["--mode"], values["--language"]
	var config struct {
		Probe int
		Fail  bool
	}
	data, _ := os.ReadFile(filepath.Join(root, "uap-fixture.json"))
	_ = json.Unmarshal(data, &config)
	log, _ := os.OpenFile(filepath.Join(root, "uap-calls"), os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
	if log == nil {
		os.Exit(8)
	}
	fmt.Fprintf(log, "%s %s\n", mode, lang)
	log.Close()
	if mode == "probe" {
		os.Exit(config.Probe)
	}
	if lang != "fr" && lang != "en" {
		os.Exit(9)
	}
	work := filepath.Join(root, "modern-ui", "uap-"+lang)
	if strings.HasSuffix(mode, "-hotfix") {
		work = filepath.Join(root, "modern-ui", "uap-hotfix", lang)
	}
	_ = os.MkdirAll(work, 0700)
	_ = os.WriteFile(filepath.Join(work, "progress"), []byte("backup 63\n"), 0600)
	time.Sleep(250 * time.Millisecond)
	fmt.Println("fixture helper log")
	if config.Fail {
		os.Exit(1)
	}
	if strings.HasPrefix(mode, "install") {
		_ = os.WriteFile(filepath.Join(work, "fixture-active"), []byte(lang), 0600)
	}
	os.Exit(0)
}
func uapFixture(t *testing.T, root string, probe int, fail bool) {
	t.Helper()
	exe, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(exe)
	if err != nil {
		t.Fatal(err)
	}
	name := "ArcanumModern/uap/uap-helper"
	if runtime.GOOS == "windows" {
		name += ".exe"
	}
	put(t, root, name, data)
	if err = os.Chmod(filepath.Join(root, name), 0755); err != nil {
		t.Fatal(err)
	}
	put(t, root, "uap-fixture.json", []byte(fmt.Sprintf(`{"Probe":%d,"Fail":%t}`, probe, fail)))
	t.Setenv("RENAISSANCE_TEST_UAP", "1")
}
func awaitOffer(t *testing.T, a *application) view {
	t.Helper()
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		v := a.snapshot()
		if v.Offer != 0 {
			return v
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatal("UAP choice was not offered")
	return view{}
}
func TestUAPPreparationEditionAndChoice(t *testing.T) {
	for _, probe := range []int{20, 21, 22, 23} {
		for _, install := range []bool{false, true} {
			t.Run(fmt.Sprintf("%d/install=%t", probe, install), func(t *testing.T) {
				root := t.TempDir()
				uapFixture(t, root, probe, false)
				a := &application{root: root, answer: make(chan bool, 1), state: view{Phase: "offline", Notes: "release notes"}}
				ctx, cancel := context.WithCancel(context.Background())
				defer cancel()
				done := make(chan struct{})
				go func() { a.prepareUAP(ctx); close(done) }()
				v := awaitOffer(t, a)
				lang := "en"
				if probe == 20 || probe == 22 {
					lang = "fr"
				}
				if v.GameLanguage != lang || v.Ready || v.autoStart() {
					t.Fatalf("unsafe offer: %+v", v)
				}
				// UI translation is independent of the language selected for game data.
				if !strings.Contains(v.notes("en"), map[string]string{"en": "English", "fr": "French"}[lang]) {
					t.Fatal(v.notes("en"))
				}
				if !strings.Contains(v.notes("fr"), map[string]string{"en": "anglais", "fr": "français"}[lang]) {
					t.Fatal(v.notes("fr"))
				}
				a.respond(install)
				a.respond(!install) // A double click cannot enqueue another action.
				select {
				case <-done:
				case <-time.After(10 * time.Second):
					t.Fatal("helper did not finish")
				}
				mode := "decline"
				if install {
					mode = "install"
				}
				if probe >= 22 {
					mode += "-hotfix"
				}
				calls, _ := os.ReadFile(filepath.Join(root, "uap-calls"))
				if string(calls) != "probe auto\n"+mode+" "+lang+"\n" {
					t.Fatal(string(calls))
				}
				v = a.snapshot()
				if v.Phase != "offline" || v.Notes != "release notes" || v.Offer != 0 {
					t.Fatalf("wrong final state: %+v", v)
				}
				work := filepath.Join(root, "modern-ui", "uap-"+lang)
				if probe >= 22 {
					work = filepath.Join(root, "modern-ui", "uap-hotfix", lang)
				}
				if exists(filepath.Join(work, "fixture-active")) != install {
					t.Fatal("incorrect activation")
				}
				log, _ := os.ReadFile(filepath.Join(work, "preparation.log"))
				if !strings.Contains(string(log), "fixture helper log") {
					t.Fatal("missing helper log")
				}
			})
		}
	}
}
func TestUAPPreparationFailureAndRememberedChoice(t *testing.T) {
	for _, probe := range []int{0, 1, 22} {
		t.Run(fmt.Sprint(probe), func(t *testing.T) {
			root := t.TempDir()
			uapFixture(t, root, probe, true)
			a := &application{root: root, answer: make(chan bool, 1), state: view{Phase: "current"}}
			done := make(chan struct{})
			go func() { a.prepareUAP(context.Background()); close(done) }()
			if probe == 22 {
				awaitOffer(t, a)
				a.respond(true)
			}
			select {
			case <-done:
			case <-time.After(10 * time.Second):
				t.Fatal("helper did not finish")
			}
			want := "uap-failed"
			if probe == 0 {
				want = "current"
			}
			v := a.snapshot()
			if v.Phase != want {
				t.Fatal(v)
			}
			v.Ready = true
			if probe != 0 && (v.autoStart() || v.Fatal) {
				t.Fatal("failure must allow manual play only")
			}
		})
	}
}
func TestSkippingUpdateStillOffersUAPAndClosingDoesNotDecline(t *testing.T) {
	root := t.TempDir()
	uapFixture(t, root, 21, false)
	a := &application{root: root, answer: make(chan bool, 1), done: make(chan struct{}), state: view{Phase: "check", Installed: "2.0.1"}}
	update, cancel := context.WithCancel(context.Background())
	cancel()
	preparation, stop := context.WithCancel(context.Background())
	defer stop()
	go a.run(update, preparation)
	awaitOffer(t, a)
	if a.snapshot().Ready {
		t.Fatal("game became ready before the choice")
	}
	stop()
	select {
	case <-a.done:
	case <-time.After(10 * time.Second):
		t.Fatal("closing did not finish")
	}
	calls, _ := os.ReadFile(filepath.Join(root, "uap-calls"))
	if string(calls) != "probe auto\n" {
		t.Fatal("closing must not remember a refusal", string(calls))
	}
}
