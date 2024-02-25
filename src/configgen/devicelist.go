package main

import (
	"flag"
	"fmt"
	"bytes"
	"gopkg.in/yaml.v3"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type DeviceDefinition struct {
	Device           string `yaml:"Device"`
	VendorID         string `yaml:"VendorID"`
	VendorName       string `yaml:"VendorName"`
	PID              string `yaml:"PID"`
	Description      string `yaml:"Description"`
	DocumentationURL string `yaml:"DocumentationURL"`
	DeviceType       string `yaml:"DeviceType"`
	Channels         int    `yaml:"Channels"`
	Notes            string `yaml:"Notes"`
	SrcFile          string `yaml:"SrcFile"`
	TestingStatus    string `yaml:"TestingStatus"`
}

var (
	pathFlag = flag.String("path", "../../documentation/devices/", "Path to *.yml files for device documentation")
	outputFile = flag.String("output", "configgen/drivers.go", "File to create")
)

func parsefile(filename string) (*DeviceDefinition, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("unable to read file: %v", filename)
	}

	entry := &DeviceDefinition{}

	err = yaml.Unmarshal(data, entry)
	if err != nil {
		return nil, fmt.Errorf("unable to decode yaml: %v", err)
	}

	return entry, nil
}

func main() {
	flag.Parse()

	entries := []*DeviceDefinition{}
	otherfiles := 0

	files, err := os.ReadDir(*pathFlag)
	if err != nil {
		log.Fatalf("Unable to read directory %q: %v", *pathFlag, err)
	}

	filenames := []string{}
	for _, file := range files {
		if strings.HasSuffix(file.Name(), ".yml") {
			filenames = append(filenames, file.Name())
		}
	}

	sort.Strings(filenames)
	for _, name := range filenames {
		if name[0] != '.' && strings.Contains(name, ".yml") {
			entry, err := parsefile(filepath.Join(*pathFlag, name))
			if err != nil {
				log.Fatalf("Unable to parse file %q: %v", name, err)
			}
			if strings.TrimSpace(entry.Description) != "UNKNOWN" {
				entries = append(entries, entry)
			} else {
				otherfiles++
			}
		}
	}

	var sb bytes.Buffer
	
	sb.WriteString("// Code generated  DO NOT EDIT.\n")
	sb.WriteString("package configgen\n")
	sb.WriteString("type EthercatDriver struct {\n")
	sb.WriteString("	VendorID string\n")
	sb.WriteString("    ProductID string\n")
	sb.WriteString("    Type string\n")
	sb.WriteString("}\n");

	sb.WriteString("var Drivers=map[string]EthercatDriver{\n")
	for _, e := range entries {
		sb.WriteString(fmt.Sprintf("  %q: EthercatDriver{\n", e.Device))
		sb.WriteString(fmt.Sprintf("    VendorID: %q,\n", e.VendorID))
		sb.WriteString(fmt.Sprintf("    ProductID: %q,\n", e.PID))
		sb.WriteString(fmt.Sprintf("    Type: %q,\n", e.Device))
		sb.WriteString("  },\n")
	}
	sb.WriteString("}\n")

	err = os.WriteFile(*outputFile, sb.Bytes(), 0644)
	if err != nil {
		panic(err)
	}
}
