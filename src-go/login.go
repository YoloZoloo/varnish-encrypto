package main

import (
	"database/sql"
	"fmt"
	"log"
	"net/http"

	_ "github.com/astaxie/session"
	_ "github.com/go-sql-driver/mysql"
)

func login(w http.ResponseWriter, r *http.Request) {
	if r.Method == "GET" {
		http.ServeFile(w, r, "login.html")
		return

	} else if r.Method == "POST" {
		if err := r.ParseForm(); err != nil {
			fmt.Fprintf(w, "ParseForm() err: %v", err)
			return
		}
		user_id := r.FormValue("user_id")
		password := r.FormValue("password")
		db, err := sql.Open("mysql", "chat:password@tcp(node3.codeatyolo.link:3306)/chat")
		// if there is an error opening the connection, handle it
		if err != nil {
			panic(err.Error())
		}
		defer db.Close()

		res, err := db.Query("SELECT count(*) as cnt FROM user_m WHERE user_id = ? and password = ?", user_id, password)
		if res.Next() {

			var cnt int
			err = res.Scan(&cnt)

			if err != nil {
				log.Fatal(err)
			}

			if cnt == 1 {
				sess := globalSessions.SessionStart(w, r)
				sess.Set("logged-in", true)
				http.ServeFile(w, r, "chatscreen/chatscreen.html")
			} else {
				http.ServeFile(w, r, "login.html")
			}
		}

		// if there is an error inserting, handle it
		if err != nil {
			panic(err.Error())
		}
		// be careful deferring Queries if you are using transactions
		defer res.Close()

		// defer the close till after the main function has finished
		// executing
		defer db.Close()

	} else {
		fmt.Fprintf(w, "BAD METHOD! \n")
	}
	return
}
