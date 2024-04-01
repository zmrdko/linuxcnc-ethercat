package main

import (
	"encoding/xml"
	"flag"
	"fmt"
	"golang.org/x/net/html/charset"
	"gopkg.in/yaml.v3"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

var (
	esiDirFlag = flag.String("esi_directory", "/tmp/esi", "Directory that contains ESI XML files")
	output     = flag.String("output", "", "Output file, defaults to stdout")
)

type ESIName struct {
	Name       string `xml:",chardata"`
	LanguageID string `xml:"LcId,attr"`
}

type ESIDeviceURL struct {
	URL        string `xml:",chardata"`
	LanguageID string `xml:"LcId,attr"`
}

type ESIDeviceInfo struct {
	Current string              `xml:"Electrical>EBusCurrent" yaml:",omitempty"`
	Ports   []ESIDeviceInfoPort `xml:"Port" yaml:",omitempty"`
}

type ESIDeviceInfoPort struct {
	Type  string
	Label string
}

type ESIDevice struct {
	//Physics string `xml:"Physics,attr" yaml:",omitempty"`

	Type struct {
		Type            string `xml:",chardata"`
		ProductCode     string `xml:"ProductCode,attr"`
		RevisionNo      string `xml:"RevisionNo,attr"`
		CheckRevisionNo string `xml:"CheckRevisionNo,attr"`
	} `xml:"Type" yaml:"-"`

	Names []ESIName      `xml:"Name" yaml:",omitempty"`
	URLs  []ESIDeviceURL `xml:"URL" yaml:",omitempty"`
	//Info      ESIDeviceInfo  `xml:"Info" yaml:",omitempty"`
	GroupType string `xml:"GroupType" yaml:",omitempty"` // maps to ESIGroup.Type
	//Fmmu      []string       `xml:"Fmmu" yaml:",omitempty,flow"` // "Inputs"/"Outputs"

	ShortType      string       `yaml:"Type"`
	ProductCode    string       `yaml:"ProductCode"`
	RevisionNo     string       `yaml:"RevisionNo"`
	EnglishURL     string       `yaml:"URL"`
	EnglishName    string       `yaml:"Name"`
	GroupName      string       `yaml:"DeviceGroup"`
	VendorName     string       `yaml:"Vendor"`
	VendorID       string       `yaml:"VendorID"`
	AssignActivate string       `yaml:"AssignActivate"`
	SyncCycle0     string       `yaml:"SyncCycle0,omitempty"`
	SyncCycle1     string       `yaml:"SyncCycle1,omitempty"`
	SyncFactor0    string       `yaml:"SyncFactor0,omitempty"`
	SyncFactor1    string       `yaml:"SyncFactor1,omitempty"`
	SyncShift0     string       `yaml:"SyncShift0,omitempty"`
	SyncShift1     string       `yaml:"SyncShift1,omitempty"`
	OpModes        []*ESIOpMode `xml:"Dc>OpMode" yaml:",omitempty"`
}

type ESIGroup struct {
	Type        string    `xml:"Type"`
	Names       []ESIName `xml:"Name"`
	EnglishName string
}

type ESIData struct {
	EtherCATInfo xml.Name  `xml:"EtherCATInfo"`
	VendorID     string    `xml:"Vendor>Id"`
	Vendors      []ESIName `xml:"Vendor>Name"`
	VendorName   string
	Groups       []*ESIGroup  `xml:"Descriptions>Groups>Group"`
	Devices      []*ESIDevice `xml:"Descriptions>Devices>Device"`
}

type ESIOpMode struct {
	//	OpMode         xml.Name `xml:"OpMode"`
	Name           string `xml:"Name"`
	Desc           string `xml:"Desc"`
	AssignActivate string `xml:"AssignActivate"`
	CycleTimeSync0 struct {
		N      xml.Name `xml:"CycleTimeSync0"`
		Value  string   `xml:",chardata"`
		Factor string   `xml:"Factor,attr"`
	}
	CycleTimeSync1 struct {
		N      xml.Name `xml:"CycleTimeSync1"`
		Value  string   `xml:",chardata"`
		Factor string   `xml:"Factor,attr"`
	}
	ShiftTimeSync0 string `xml:"ShiftTimeSync0"`
	ShiftTimeSync1 string `xml:"ShiftTimeSync1"`
}

func parseESIFile(filename string) ([]*ESIDevice, error) {
	f, err := os.Open(filename)
	if err != nil {
		return nil, fmt.Errorf("unable to open file: %v", err)
	}

	esiData := &ESIData{}
	devices := []*ESIDevice{}

	decoder := xml.NewDecoder(f)
	decoder.CharsetReader = charset.NewReaderLabel
	err = decoder.Decode(esiData)
	if err != nil {
		return nil, fmt.Errorf("unable to parse xml file %q: %v", filename, err)
	}

	groupMap := make(map[string]string)

	// Beckhoff's ESI groups have names in DE and EN; we want the English names.
	for _, g := range esiData.Groups {
		for _, n := range g.Names {
			if n.LanguageID == "1033" { // English
				g.EnglishName = n.Name
				groupMap[g.Type] = g.EnglishName
			}
		}
	}

	vendor := "Unknown"
	if len(esiData.Vendors) > 0 {
		vendor = esiData.Vendors[0].Name
		for _, v := range esiData.Vendors {
			if v.LanguageID == "1033" { // English
				vendor = v.Name
			}
		}
	}

	// We have a fair amount of post-processing to do per Device.
	//
	// - We want to get the English name and URLs
	// - We want to include the vendor and vendor ID (correctly formatted)
	// - We want to match the group name to the group type provided
	// - We want to reformat the product code and revision to be 0xXXX-format hex numbers instead of #xXXX
	// - We want to strip extra data (non-English names and URLs, group data, etc
	for _, d := range esiData.Devices {
		d.VendorID = fixHexFormat(esiData.VendorID, 8)
		d.VendorName = vendor

		for _, n := range d.Names {
			if n.LanguageID == "1033" { // English
				d.EnglishName = strings.TrimSpace(n.Name)
			}
		}
		for _, u := range d.URLs {
			if u.LanguageID == "1033" { // English
				d.EnglishURL = u.URL
			}
		}
		for _, o := range d.OpModes {
			a := fixHexFormat(o.AssignActivate, 3)
			aa, _ := strconv.ParseInt(a, 0, 64)

			if aa >= 0x300 {
				d.AssignActivate = a
				d.SyncShift0 = o.ShiftTimeSync0
				d.SyncShift1 = o.ShiftTimeSync1
				d.SyncCycle0 = o.CycleTimeSync0.Value
				d.SyncFactor0 = o.CycleTimeSync0.Factor
				d.SyncCycle1 = o.CycleTimeSync1.Value
				d.SyncFactor1 = o.CycleTimeSync1.Factor
				break
			}
		}

		d.OpModes = nil

		d.Type.ProductCode = fixHexFormat(d.Type.ProductCode, 8)
		d.Type.RevisionNo = fixHexFormat(d.Type.RevisionNo, 8)
		d.GroupName = groupMap[d.GroupType]
		d.ShortType = d.Type.Type
		d.ProductCode = d.Type.ProductCode
		d.RevisionNo = d.Type.RevisionNo
		d.GroupType = ""
		d.Names = nil
		d.URLs = nil

		devices = append(devices, d)
	}
	return devices, nil
}

func fixHexFormat(in string, nibbles int) string {
	// No need to "fix" empty strings
	if len(in) == 0 {
		return in
	}

	// XML likes #xXXXX, change to 0xXXX
	if in[0] == '#' {
		out := []rune(in)
		out[0] = '0'
		v, err := strconv.ParseInt(string(out), 0, 64)
		if err != nil {
			panic(err)
		}
		return fmt.Sprintf("0x%0*x", nibbles, v)
	}

	// Okay, it's probably just an integer.  Beckhoff likes that.  Convert to hex.
	i, err := strconv.Atoi(in)
	if err != nil {
		panic(err)
	}

	return fmt.Sprintf("0x%0*x", nibbles, i)
}

func main() {
	flag.Parse()

	devices := []*ESIDevice{}

	files, err := os.ReadDir(*esiDirFlag)
	if err != nil {
		panic(err)
	}

	for _, file := range files {
		if strings.HasSuffix(file.Name(), ".xml") {
			fmt.Fprintf(os.Stderr, "Parsing %s... ", file.Name())

			d, err := parseESIFile(filepath.Join(*esiDirFlag, file.Name()))
			if err != nil {
				panic(err)
			}

			fmt.Fprintf(os.Stderr, "%d devices\n", len(d))

			devices = append(devices, d...)
		}
	}

	y, err := yaml.Marshal(devices)
	if err != nil {
		panic(err)
	}

	if *output != "" {
		err = os.WriteFile(*output, y, 0644)
		if err != nil {
			panic(err)
		}
	} else {
		fmt.Println(string(y))
	}
}
