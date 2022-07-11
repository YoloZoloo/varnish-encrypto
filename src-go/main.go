package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"regexp"

	"github.com/astaxie/session"

	_ "github.com/astaxie/session"
	_ "github.com/astaxie/session/providers/memory"

	_ "github.com/go-sql-driver/mysql"
)

// type apiHandler struct{}

// func (apiHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
// 	if r.Method == "GET" {

// 	} else if r.Method == "POST" {
// 		if err := r.ParseForm(); err != nil {
// 			fmt.Fprintf(w, "ParseForm() err: %v", err)
// 			return
// 		}
// 		user_id := r.FormValue("user_id")
// 		password := r.FormValue("password")
// 		db, err := sql.Open("mysql", "root:password@tcp(localhost:3306)")

// 	}
// }

// func loadPage(title string) *Page {
// 	filename := title + ".txt"
// 	body, _ := os.ReadFile(filename)
// 	return &Page{Title: title, Body: body}
// }

var globalSessions *session.Manager

var rChat = regexp.MustCompile(`^/chatscreen`)
var rEdit = regexp.MustCompile(`^/edit`)

func showDefaultIcon(w http.ResponseWriter, r *http.Request) {
	buf, err := ioutil.ReadFile("common/defaultIcon.png")
	if err != nil {
		log.Fatal(err)
	}
	w.Header().Set("Content-Type", "image/png")
	w.Write(buf)
}
func route(w http.ResponseWriter, r *http.Request) {
	switch {
	case rChat.MatchString(r.URL.Path):
		getchatscreen(w, r)
	case rEdit.MatchString(r.URL.Path):
		geteditscreen(w, r)
	default:
		http.Error(w, "Method is not supported.", http.StatusNotFound)
	}
}

func main() {
	globalSessions, _ = session.NewManager("memory", "gosessionid", 3600)
	fmt.Printf("Starting server at port 8081\n")
	http.HandleFunc("/login", login)
	http.HandleFunc("/defaultIcon.png", showDefaultIcon)
	http.HandleFunc("/", route)

	if err := http.ListenAndServe(":8081", nil); err != nil {
		log.Fatal(err)
	}
}
