@echo off
set CGO_ENABLED=0
set GOARCH=amd64
set GOAMD=v1
set GOOS=windows
go build -ldflags="-s -w -X main.version=stripped -buildid= -extldflags=static" -buildvcs=false -trimpath -a -o qbp-go.exe
set GOOS=linux
go build -ldflags="-s -w -X main.version=stripped -buildid= -extldflags=static" -buildvcs=false -trimpath -a -o qbp-go
