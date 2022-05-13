package pwmgenerator

import (
	"errors"
	"fmt"
	"net"
	"strings"
)

type Type byte

const (
	CST Type = iota
	SIN
	TRI
)

var (
	ErrInputBufferOverflow = errors.New("input buffer overflow")
	ErrEmptyRequest        = errors.New("empty request")
	ErrInvalidGreeting     = errors.New("invalid greeting")
	ErrNotConnected        = errors.New("not connected")
	ErrEmptyResponse       = errors.New("empty response")
	ErrInvalidResponse     = errors.New("invalid response")
)

const maxBufferSize = 65536

func (t Type) String() string {
	switch t {
	case CST:
		return "CST"
	case SIN:
		return "SIN"
	case TRI:
		return "TRI"
	}
	return "?"
}

type Param struct {
	Type      Type
	Average   float64
	Amplitude float64
	Period    float64
	Start     float64
}

type PWMGenerator struct {
	conn     net.Conn
	buf      []byte
	beg, end int
	err      error
	nChan    int
}

// New returns a new PWMGenerator.
func New() *PWMGenerator {
	return &PWMGenerator{}
}

// NotFatalError is an error that is not fatal for the communication
// with the PWMGenerator. A new request may be issued. The method
// Error() returns a non-nil value when the last error was fatal.
type NotFatalError struct {
	msg string
	err error
}

// Unwrap returns the enclosed error.
func (n NotFatalError) Unwrap() error {
	return n.err
}

// Returns the error as a string developping the chaining.
func (n NotFatalError) Error() string {
	if n.err != nil {
		if n.msg == "" {
			return n.err.Error()
		}
		return n.msg + ": " + n.err.Error()
	}
	return n.msg
}

// IsFatal returns true if the error was not returned by the PWMGenerator.
func IsFatal(err error) bool {
	var r NotFatalError
	return !errors.As(err, &r)
}

// dataOrError extract error from response or valid
// response.
func (p *PWMGenerator) dataOrError(rsp []byte) ([]byte, error) {
	// fmt.Println("debug: recvRsp:")
	// fmt.Println(hex.Dump(rsp))
	if len(rsp) == 0 {
		p.err = ErrEmptyResponse
		return nil, p.err
	}
	if rsp[0] == '!' {
		return nil, NotFatalError{msg: string(rsp[1:])}
	}
	if rsp[0] == '>' {
		return rsp[1:], nil
	}
	p.err = ErrInvalidResponse
	return nil, p.err
}

// recvRsp returns the next line received from the PWMgenerator.
func (p *PWMGenerator) recvRsp() ([]byte, error) {
	if p.err != nil {
		return nil, p.err
	}
	if p.buf == nil {
		p.buf = make([]byte, 1024)
		p.beg = 0
		p.end = 0
	}
	// pop previous message
	copy(p.buf, p.buf[p.beg:p.end])
	p.end = p.end - p.beg
	p.beg = 0
	// locate next newline in buffered data
	for p.beg = 0; p.beg < p.end; p.beg++ {
		if p.buf[p.beg] == '\n' {
			p.beg++
			return p.dataOrError(p.buf[:p.beg])
		}
	}
	for len(p.buf) < maxBufferSize {
		// grow input buffer when needed
		if p.end == len(p.buf) {
			tmp := make([]byte, cap(p.buf)*2)
			copy(tmp, p.buf)
			p.buf = tmp
		}
		for p.end < len(p.buf) {
			// TODO: set a read timeout
			n, err := p.conn.Read(p.buf[p.end:])
			if err != nil {
				p.err = err
				return nil, err
			}
			p.end += n
			// locate newline in received data
			for ; p.beg < p.end; p.beg++ {
				if p.buf[p.beg] == '\n' {
					p.beg++
					return p.dataOrError(p.buf[:p.beg])
				}
			}
		}
	}
	return nil, ErrInputBufferOverflow
}

func (p *PWMGenerator) sendReq(format string, a ...interface{}) error {
	if p.err != nil {
		return p.err
	}
	s := strings.Trim(fmt.Sprintf(format, a...), " ")
	if len(s) == 0 {
		return NotFatalError{err: ErrEmptyRequest}
	}
	if s[len(s)-1] != '\n' {
		s += "\n"
	}
	//	fmt.Printf("debug: sendReq: %q\n", s)
	for len(s) > 0 {
		n, err := p.conn.Write([]byte(s))
		if err != nil {
			p.err = err
			return err
		}
		s = s[n:]
	}
	return nil
}

func (p *PWMGenerator) Error() error {
	return p.err
}

// Close the PWMGenerator. It is required in case of fatal error.
// An error is fatal if the Error() method returns a none nil value.
//
func (p *PWMGenerator) Close() error {
	conn := p.conn
	p.conn = nil
	p.beg = 0
	p.end = 0
	p.err = nil
	if conn != nil {
		return NotFatalError{err: conn.Close()}
	}
	return nil
}

func (p *PWMGenerator) Open(addr string) (string, error) {
	if p.conn != nil || p.err != nil {
		p.Close()
	}
	p.conn, p.err = net.Dial("tcp", addr)
	if p.err != nil {
		return "", p.err
	}
	if err := p.sendReq("PWM0\n"); err != nil {
		return "", err
	}
	rsp, err := p.recvRsp()
	if err != nil {
		var notFatalErr NotFatalError
		if errors.As(err, &notFatalErr) {
			if notFatalErr.err != nil {
				p.err = notFatalErr
			} else {
				p.err = errors.New(notFatalErr.msg)
			}
			return "", p.err
		}
		return "", err
	}
	if !strings.HasPrefix(string(rsp), "HELO ") {
		p.err = ErrInvalidGreeting
		return "", p.err
	}
	return strings.Trim(string(rsp[5:]), " "), nil
}

func (p Param) String() string {
	return fmt.Sprintf("type=%v average=%f amplitude=%f period=%f start=%f", p.Type.String(), p.Average, p.Amplitude, p.Period, p.Start)
}

// Params returns the parameters of all the channels of the PWM generator.
// When err is not nil, a fatal error occured and the PWM generator must be
// closed. When errMsg is not empty, param and err are nil. errMgs is the
// error returned by the PWMgenerator. It is not fatal.
func (p *PWMGenerator) Params() ([]Param, error) {
	if p.conn == nil {
		p.err = ErrNotConnected
	}
	if p.err != nil {
		return nil, p.err
	}
	if err := p.sendReq("GPRM\n"); err != nil {
		return nil, p.err
	}
	rsp, err := p.recvRsp()
	if err != nil {
		return nil, err
	}
	r := strings.NewReader(string(rsp))
	var nchan int
	n, err := fmt.Fscanf(r, "%d", &nchan)
	if err != nil {
		// fmt.Println("debug: GetParams: scanf 1:", err)
		return nil, err
	}
	if n != 1 {
		return nil, NotFatalError{err: ErrInvalidResponse}
	}
	prm := make([]Param, nchan)
	for ch := range prm {
		var s string
		var chNo int
		n, err := fmt.Fscanf(r, ", %d %s %g %g %g %g", &chNo, &s, &prm[ch].Average, &prm[ch].Amplitude, &prm[ch].Period, &prm[ch].Start)
		if err != nil || n != 6 {
			// fmt.Println("debug: GetParams: scanf 2:", err)
			return nil, NotFatalError{err: ErrInvalidResponse}
		}
		switch s {
		case "CST":
			prm[ch].Type = CST
		case "SIN":
			prm[ch].Type = SIN
		case "TRI":
			prm[ch].Type = TRI
		default:
			return nil, NotFatalError{err: ErrInvalidResponse}
		}
	}
	p.nChan = nchan
	return prm, nil
}

// SetParams sets the parameters in the map m where the key is the channel
// number. The operation fails if the channel does not exist.
func (p *PWMGenerator) SetParams(m map[int]Param) error {
	if _, err := p.Params(); err != nil {
		return err
	}
	var buf strings.Builder
	buf.WriteString(fmt.Sprintf("SPRM %d", len(m)))
	for k, v := range m {
		buf.WriteString(fmt.Sprintf(", %d %v %g %g %g %g", k, v.Type.String(), v.Average, v.Amplitude, v.Period, v.Start))
	}
	buf.WriteByte('\n')
	if err := p.sendReq(buf.String()); err != nil {
		return err
	}
	rsp, err := p.recvRsp()
	if err != nil {
		return err
	}
	if string(rsp) == "DONE\n" {
		return nil
	}
	return NotFatalError{err: ErrInvalidResponse}
}

// Frequency returns the pulse generation frequency and its standard
// deviation. The first value is the frequency.
func (p *PWMGenerator) Frequency() (float64, float64, error) {
	if p.conn == nil {
		p.err = ErrNotConnected
	}
	if p.err != nil {
		return 0, 0, p.err
	}
	if err := p.sendReq("FREQ"); err != nil {
		return 0, 0, err
	}
	rsp, err := p.recvRsp()
	if err != nil {
		return 0, 0, err
	}
	r := strings.NewReader(string(rsp))
	var frequencyMean, frequencyStdDev float64
	n, err := fmt.Fscanf(r, "%g %g", &frequencyMean, &frequencyStdDev)
	if err != nil {
		// fmt.Println("debug: Frequency: scanf:", err)
		return 0, 0, err
	}
	if n != 2 {
		return 0, 0, NotFatalError{err: ErrInvalidResponse}
	}
	return frequencyMean, frequencyStdDev, nil
}
