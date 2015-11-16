// TCPServer.swift
//
// The MIT License (MIT)
//
// Copyright (c) 2015 Zewo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

struct TCPServer : TCPServerType {
    let port: Int
    let semaphore = Semaphore(resourceCount: 0)

    func acceptClient(completion: (stream: TCPStreamType?, error: ErrorType?) -> Void) {
        do {
            let ip = try IP(port: port)
            let socket = try TCPServerSocket(ip: ip, backlog: 128)

            defaultQueue.async {
                var errorCount = 0
                let maxErrors = 10
                while true {
                    do {
                        let clientSocket = try socket.accept()
                        errorCount = 0
                        let socketStream = TCPStream(socket: clientSocket)
                        completion(stream: socketStream, error: nil)
                    } catch {
                        completion(stream: nil, error: error)
                        ++errorCount
                        if errorCount == maxErrors {
                            self.stop()
                            break
                        }
                    }
                }
            }
            
            semaphore.wait()
        } catch {
            completion(stream: nil, error: error)
        }
    }

    func stop() {
        semaphore.signal()
    }
}
