// SpectrumTests.swift
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

import XCTest
import Spell

class SpectrumTests: XCTestCase {
    func test() {
        let router = HTTPRouter { router in
            router.get("/user/:id") { context in
                let id = context.parameters["id"]
                XCTAssert(id == "paulo")
                context.send(HTTPResponse(statusCode: 200, reasonPhrase: "OK"))
            }
        }

        let server = HTTPServer(port: 8080, responder: router)
        server.start()

        var request = HTTPRequest(
            method: .GET,
            uri: URI(string: "/user/paulo")
        )

        router.respond(request) { response in
            XCTAssert(response.statusCode == 200)
        }

        request = HTTPRequest(
            method: .GET,
            uri: URI(string: "/")
        )

        router.respond(request) { response in
            XCTAssert(response.statusCode == 404)
        }

    }
}
