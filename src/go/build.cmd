@echo off
set CGO_ENABLED=0
set GOEXPERIMENT=norandomizedheapbase64
set GOARCH=amd64
set GOAMD64=v1
set GOOS=windows
go build -ldflags="-s -w -X main.version=stripped -buildid= -extldflags=static" -gcflags="-B" -buildvcs=false -trimpath -o qbp-go.exe
set GOOS=linux
go build -ldflags="-s -w -X main.version=stripped -buildid= -extldflags=static" -gcflags="-B" -buildvcs=false -trimpath -o qbp-go
