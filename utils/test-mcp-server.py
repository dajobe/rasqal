#!/usr/bin/env python3
"""
MCP Server Smoke Tests

A simple test suite to verify the Rasqal MCP server is working correctly.

This is a basic smoke test - not comprehensive, just enough to verify
the server is responding with valid JSON for each tool.

In particular it will only work in the source tree since it hardcodes
paths to test data files and binaries.

"""

import json
import subprocess
import sys
import time
from typing import Dict, Any, Optional


class MCPServerTester:
    def __init__(self, server_path: str = "./rasqal-mcp-server"):
        self.server_path = server_path
        self.server_process = None

    def start_server(self) -> bool:
        """Start the MCP server process"""
        try:
            print(f"  Starting server: {self.server_path}")
            self.server_process = subprocess.Popen(
                [self.server_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=0,  # Unbuffered
            )

            # Wait for server to start
            time.sleep(0.2)

            # Check if process is still running
            if self.server_process.poll() is not None:
                print("  ❌ Server process exited unexpectedly")
                return False

            print("  ✅ Server started successfully")

            print("  ✅ Server initialized successfully")
            return True
        except Exception as e:
            print(f"Failed to start server: {e}")
            return False

    def stop_server(self):
        """Stop the MCP server process"""
        if self.server_process:
            self.server_process.terminate()
            try:
                self.server_process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                print("  ⚠️  Server didn't terminate gracefully, killing...")
                self.server_process.kill()
                self.server_process.wait()

    def send_request(
        self, request: Dict[str, Any], timeout: float = 5.0
    ) -> Optional[Dict[str, Any]]:
        """Send a JSON-RPC request to the server and return the response"""
        if not self.server_process:
            return None

        try:
            import select
            import signal

            def timeout_handler(signum, frame):
                raise TimeoutError("Timeout in send_request")

            # Set up timeout for the entire operation
            signal.signal(signal.SIGALRM, timeout_handler)
            signal.alarm(int(timeout) + 1)  # Add 1 second buffer

            try:
                # Send request
                request_str = json.dumps(request) + "\n"
                self.server_process.stdin.write(request_str)
                self.server_process.stdin.flush()

                # Check if stdout has data available
                ready, _, _ = select.select(
                    [self.server_process.stdout], [], [], timeout
                )
                if not ready:
                    print(f"  ⏰ Timeout waiting for response after {timeout}s")
                    return None

                response = self.server_process.stdout.readline()
                if response:
                    try:
                        return json.loads(response)
                    except json.JSONDecodeError as e:
                        print(f"  ❌ Invalid JSON response: {e}")
                        print(f"  Raw response: {response[:100]}...")
                        return None
                return None
            finally:
                signal.alarm(0)  # Cancel timeout

        except TimeoutError:
            print(f"  ⏰ Operation timed out after {timeout}s")
            return None
        except Exception as e:
            print(f"Error sending request: {e}")
            return None

    def test_initialize(self) -> bool:
        """Test the initialize method"""
        print("Testing initialize...")

        request = {"jsonrpc": "2.0", "method": "initialize", "id": 1}

        response = self.send_request(request)
        if not response:
            print("  ❌ No response received")
            return False

        # Check basic structure
        if "jsonrpc" not in response or "result" not in response:
            print("  ❌ Invalid JSON-RPC response structure")
            return False

        if "capabilities" not in response["result"]:
            print("  ❌ Missing capabilities in result")
            return False

        if "tools" not in response["result"]["capabilities"]:
            print("  ❌ Missing tools in capabilities")
            return False

        tools = response["result"]["capabilities"]["tools"]
        # According to MCP spec, initialize response has tools: {"listChanged": true}
        if not isinstance(tools, dict) or "listChanged" not in tools:
            print("  ❌ Invalid tools structure in capabilities")
            return False

        print(f"  ✅ initialize working - tools listChanged: {tools['listChanged']}")
        return True

    def test_tools_list(self) -> bool:
        """Test the tools/list method"""
        print("Testing tools/list...")

        request = {"jsonrpc": "2.0", "method": "tools/list", "id": 1}

        response = self.send_request(request)
        if not response:
            print("  ❌ No response received")
            return False

        # Check basic structure
        if "jsonrpc" not in response or "result" not in response:
            print("  ❌ Invalid JSON-RPC response structure")
            return False

        if "tools" not in response["result"]:
            print("  ❌ Missing tools in result")
            return False

        tools = response["result"]["tools"]
        expected_tools = [
            "execute_sparql_query",
            "validate_sparql_query",
            "list_formats",
        ]

        for tool_name in expected_tools:
            if not any(tool["name"] == tool_name for tool in tools):
                print(f"  ❌ Missing expected tool: {tool_name}")
                return False

        print(f"  ✅ tools/list working - found {len(tools)} tools")
        return True

    def test_list_formats(self) -> bool:
        """Test the list_formats tool"""
        print("Testing list_formats...")

        request = {
            "jsonrpc": "2.0",
            "method": "tools/call",
            "params": {"name": "list_formats", "arguments": {}},
            "id": 1,
        }

        response = self.send_request(request)
        if not response:
            print("  ❌ No response received")
            return False

        # Check basic structure
        if "jsonrpc" not in response or "result" not in response:
            print("  ❌ Invalid JSON-RPC response structure")
            return False

        result = response["result"]
        required_fields = ["rdf_formats", "result_formats", "query_languages"]

        for field in required_fields:
            if field not in result:
                print(f"  ❌ Missing field: {field}")
                return False

        if (
            not isinstance(result["rdf_formats"], list)
            or len(result["rdf_formats"]) == 0
        ):
            print("  ❌ No RDF formats returned")
            return False

        if (
            not isinstance(result["result_formats"], list)
            or len(result["result_formats"]) == 0
        ):
            print("  ❌ No result formats returned")
            return False

        print(
            f"  ✅ list_formats working - {len(result['rdf_formats'])} RDF formats, {len(result['result_formats'])} result formats"
        )
        return True

    def test_validate_sparql_query(self) -> bool:
        """Test the validate_sparql_query tool"""
        print("Testing validate_sparql_query...")

        # Test valid query
        request = {
            "jsonrpc": "2.0",
            "method": "tools/call",
            "params": {
                "name": "validate_sparql_query",
                "arguments": {"query": "SELECT ?s ?p ?o WHERE { ?s ?p ?o }"},
            },
            "id": 1,
        }

        response = self.send_request(request)
        if not response:
            print("  ❌ No response received")
            return False

        # Check basic structure
        if "jsonrpc" not in response or "result" not in response:
            print("  ❌ Invalid JSON-RPC response structure")
            return False

        result = response["result"]
        if "valid" not in result or not result["valid"]:
            print("  ❌ Valid query marked as invalid")
            return False

        if "query_type" not in result:
            print("  ❌ Missing query_type in result")
            return False

        print(
            f"  ✅ validate_sparql_query working - query type: {result['query_type']}"
        )
        return True

    def test_execute_sparql_query(self) -> bool:
        """Test the execute_sparql_query tool"""
        print("Testing execute_sparql_query...")

        request = {
            "jsonrpc": "2.0",
            "method": "tools/call",
            "params": {
                "name": "execute_sparql_query",
                "arguments": {
                    "query": "SELECT * WHERE { ?s ?p ?o } LIMIT 2",
                    "data_graphs": [
                        {
                            "uri": "../data/animals.nt",
                            "format": "ntriples",
                            "type": "background",
                        }
                    ],
                    "result_format": "json",
                },
            },
            "id": 1,
        }

        response = self.send_request(request)
        if not response:
            print("  ❌ No response received")
            return False

        # Check basic structure
        if "jsonrpc" not in response or "result" not in response:
            print("  ❌ Invalid JSON-RPC response structure")
            return False

        result = response["result"]
        if "output" not in result or "format" not in result:
            print("  ❌ Missing output or format in result")
            return False

        # Try to parse the output as JSON to verify it's valid
        try:
            output_json = json.loads(result["output"])
            if "head" not in output_json or "results" not in output_json:
                print("  ❌ Invalid SPARQL results format")
                return False
        except json.JSONDecodeError:
            print("  ❌ Output is not valid JSON")
            return False

        print(f"  ✅ execute_sparql_query working - format: {result['format']}")
        return True

    def run_all_tests(self) -> bool:
        """Run all tests and return overall success"""
        print("🚀 Starting MCP Server Smoke Tests")
        print("=" * 50)

        if not self.start_server():
            print("❌ Failed to start server")
            return False

        try:
            tests = [
                self.test_initialize,
                self.test_tools_list,
                self.test_list_formats,
                self.test_validate_sparql_query,
                self.test_execute_sparql_query,
            ]

            passed = 0
            total = len(tests)

            for test in tests:
                try:
                    if test():
                        passed += 1
                    else:
                        print("  ❌ Test failed")
                except Exception as e:
                    print(f"  💥 Test crashed with error: {e}")
                print()

            print("=" * 50)
            print(f"📊 Test Results: {passed}/{total} tests passed")

            if passed == total:
                print("🎉 All tests passed! MCP server is working correctly.")
                return True
            else:
                print("⚠️  Some tests failed. Check the output above for details.")
                return False

        finally:
            self.stop_server()


def main():
    """Main entry point"""
    if len(sys.argv) > 1:
        server_path = sys.argv[1]
    else:
        server_path = "./rasqal-mcp-server"

    tester = MCPServerTester(server_path)
    success = tester.run_all_tests()

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
