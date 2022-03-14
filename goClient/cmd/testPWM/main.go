package main

import (
	"fmt"
	"goClient/pwmgenerator"
	"os"
	"time"
)

var config = map[int]pwmgenerator.Param{
	0: {Type: pwmgenerator.CST, Average: .1},
	1: {Type: pwmgenerator.TRI, Average: .5, Amplitude: .5, Period: 0.003},
	2: {Type: pwmgenerator.SIN, Average: .5, Amplitude: .5, Period: 1},
	3: {Type: pwmgenerator.SIN, Average: .5, Amplitude: .5, Period: 1},
	4: {Type: pwmgenerator.SIN, Average: .5, Amplitude: .5, Period: 1},
	5: {Type: pwmgenerator.SIN, Average: .5, Amplitude: .5, Period: 1},
	6: {Type: pwmgenerator.SIN, Average: .5, Amplitude: .5, Period: 1},
	7: {Type: pwmgenerator.SIN, Average: .5, Amplitude: .5, Period: 1},
}

func main() {
	p := pwmgenerator.New()

	var addr string
	if len(os.Args) == 1 {
		addr = "127.0.0.1:1234"
	} else {
		addr = os.Args[1]
	}
	fmt.Println("try connecting to", addr)

	info, err := p.Open(addr)
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	defer p.Close()
	fmt.Println("info:", info)

	// get configuration
	params, err := p.Params()
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	for i := range params {
		fmt.Println(params[i])
	}

	// get and display frequency
	frequencyMean, frequencyStdDev, err := p.Frequency()
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	fmt.Printf("frequency: mean=%g Hz stdDev=%g\n", frequencyMean, frequencyStdDev)

	// set configuration
	if err := p.SetParams(config); err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}

	// get configuration
	params, err = p.Params()
	if err != nil {
		fmt.Println("error:", err)
		os.Exit(1)
	}
	for i := range params {
		fmt.Println(params[i])
	}

	for i := 0; i < 500; i++ {
		time.Sleep(3 * time.Second)
		// get and display frequency
		frequencyMean, frequencyStdDev, err := p.Frequency()
		if err != nil {
			fmt.Println("error:", err)
			os.Exit(1)
		}
		fmt.Printf("frequency: mean=%g Hz stdDev=%g\n", frequencyMean, frequencyStdDev)
	}
}
