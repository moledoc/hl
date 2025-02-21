package main

import (
	"fmt"
	"strings"
)

// NOTE: it's an english sentence
//NOTE: it's an english sentence
/*
NOTE: it's an english sentence
*/
/* NOTE: it's an english sentence */
/*NOTE: it's an english sentence*/

func main() {
	var c byte
	c = 'a'
	if 'b' == c {
		fmt.Println("")
	}
	fmt.Printf("%v\n", strings.ToLower("hello WORLD"))
}
