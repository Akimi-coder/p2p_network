package main

import (
	"fmt"
	"net"
	"time"
)

func main() {
	msg := fmt.Sprintf("TESTING")
	msgPrefix := "some-prefix-to-prevent-arbitrary-connection"
	
	c, _ := net.Dial("tcp", "127.0.0.1:12345")
	for i := 0; i < 10; i++ {
		c.Write([]byte(fmt.Sprintf("%s%s", msgPrefix, msg)))
		time.Sleep(time.Millisecond * time.Duration(100))
	}
	c.Write([]byte("This won't be parsed"))
	c.Close()
}