# Rasqal MCP Server

A reference implementation of a Model Context Protocol (MCP) server that demonstrates how to expose SPARQL query capabilities to AI agents. This serves as both a practical learning exercise for MCP protocol implementation and a working example of integrating Rasqal with modern AI tooling.

## Purpose & Scope

This is designed as **educational example code** rather than a production utility:

**Learning Objectives:**

* Understand MCP protocol implementation in C
* Explore Rasqal library integration patterns
* Demonstrate JSON-RPC over stdin/stdout patterns
* Show how to expose C libraries to AI agents

**Reference Value:**

* Template for other MCP servers in C
* Working example beyond simple demos
* Practical use of autotools, YAJL, and Rasqal stack

**Personal Utility:**

* All proposed tools would be genuinely useful for SPARQL development
* Local RDF file testing with AI assistance
* Query validation capabilities

## Implementation Approach

**Location: utils/ directory** as a reference implementation

**Rationale:**

* Educational focus - learning MCP protocol and Rasqal integration
* Self-contained example with clear documentation  
* No production concerns (performance, security, scalability)
* Focused on code clarity and educational value

**Code Sharing Strategy:**

* Reuse existing Rasqal/Raptor functionality directly
* Leverage `rasqal_cmdline_read_data_graph` from utils/
* Simple, readable implementation over optimization
* Well-commented code explaining MCP concepts

## MCP Server Capabilities

### Tools (AI-callable functions)

#### `execute_sparql_query`

Execute a SPARQL query against loaded data graphs.

Parameters:

* `query` (string): SPARQL query text
* `data_graphs` (array): RDF data sources
  * `uri` (string): Data source URI/path
  * `format` (string, optional): RDF format (turtle, rdfxml, etc.)
  * `type` (string): "background" or "named"
  * `name` (string, optional): Named graph URI
* `result_format` (string, optional): Output format (json, xml, csv, etc.)
* `query_language` (string, optional): Query language (sparql, defaults to sparql)

Returns:

* JSON SPARQL results format for SELECT/ASK queries
* RDF serialization for CONSTRUCT/DESCRIBE queries
* Error details for query failures

#### `validate_sparql_query`

Parse and validate SPARQL query syntax without execution.

Parameters:

* `query` (string): SPARQL query text
* `query_language` (string, optional): Query language

Returns:

* `valid` (boolean): Whether query is syntactically valid
* `errors` (array): Parse error details with locations
* `query_type` (string): SELECT/CONSTRUCT/ASK/DESCRIBE
* `variables` (array): Query variables found (including SELECT * variables)

#### `list_formats`

List supported RDF input formats and result output formats with detailed descriptions.

Returns:

* `rdf_formats` (array): Available RDF parsers with descriptions
  * `name` (string): Format identifier (e.g., "turtle", "rdfxml")
  * `description` (string): Human-readable description of the format
* `result_formats` (array): Available result serializers with descriptions
  * `name` (string): Format identifier (e.g., "json", "csv")
  * `description` (string): Human-readable description of the format
* `query_languages` (array): Supported query languages with descriptions
  * `name` (string): Language identifier (e.g., "sparql")
  * `description` (string): Human-readable description of the language

### Format Support

The server dynamically discovers all supported formats from the underlying Raptor and Rasqal libraries:

1. RDF Input Formats
2. Result Output Formats
3. Query Languages

## Building

The MCP server is built as part of the standard Rasqal build process:

```bash
# From the top-level directory
./autogen.sh
./configure
make
```

The binary will be created as `utils/rasqal-mcp-server`.

## Usage

### Basic Usage

The MCP server communicates via stdin/stdout using JSON-RPC 2.0:

```bash
# Launch the server
./utils/rasqal-mcp-server

# Send a request (from another process)
echo '{"jsonrpc":"2.0","method":"tools/list","id":1}' | ./utils/rasqal-mcp-server
```

### Command-Line Options

```bash
./utils/rasqal-mcp-server --help
```

Available options:

* `-h, --help`: Print help information and exit
* `-v, --version`: Print version information and exit
* `-q, --quiet`: Suppress non-error messages
* `-d, --debug`: Enable debug output
* `-l, --log-file FILE`: Write log output to file

### Testing with Example Data

```bash
# Test with a simple SPARQL query
echo '{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "id": 1,
  "params": {
    "name": "execute_sparql_query",
    "arguments": {
      "query": "SELECT ?s ?p ?o WHERE { ?s ?p ?o } LIMIT 5",
      "data_graphs": [
        {"uri": "data/animals.nt", "format": "ntriples", "type": "background"}
      ],
      "result_format": "json"
    }
  }
}' | ./utils/rasqal-mcp-server
```

### Format Discovery

```bash
# List all supported formats with detailed descriptions
echo '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"list_formats","arguments":{}},"id":1}' | ./utils/rasqal-mcp-server
```

### Manual Testing

You can test the server manually by running it and typing JSON-RPC requests:

```bash
./utils/rasqal-mcp-server
```

Then type requests like:

```json
{"jsonrpc":"2.0","method":"tools/list","id":1}
```

## Example Usage Scenarios

### Automated Testing

```json
{
  "tool": "execute_sparql_query",
  "parameters": {
    "query": "SELECT ?s ?p ?o WHERE { ?s ?p ?o } LIMIT 10",
    "data_graphs": [
      {"uri": "test-data.ttl", "format": "turtle", "type": "background"}
    ],
    "result_format": "json"
  }
}
```

### Query Development Assistance

```json
{
  "tool": "validate_sparql_query",
  "parameters": {
    "query": "SELECT ?name WHERE { ?person foaf:name ?name"
  }
}
```

## Implementation Status

### ✅ Completed Features

1. **MCP Server Framework**: Full JSON-RPC 2.0 implementation with MCP specification compliance
2. **Command-Line Interface**: Comprehensive option parsing and help system
3. **Tool Discovery**: `tools/list` method working correctly
4. **Enhanced Format Discovery**: Dynamic format detection with detailed descriptions
5. **SPARQL Execution**: Basic query execution with result formatting
6. **Query Validation**: Syntax checking, query type detection, and variable extraction
7. **Error Handling**: Proper JSON-RPC error responses with correct ID handling
8. **Memory Management**: Clean resource handling and cleanup
9. **File Logging**: Comprehensive logging system with `--log-file` support
10. **MCP Protocol Compliance**: Full support for initialize, notifications, and tool responses
11. **Enhanced Tool Responses**: Rich output with both content and structuredContent

### 🎯 Current Capabilities

* **Server Initialization**: Proper MCP protocol initialization with capabilities and instructions
* **Tool Listing**: Complete tool catalog with input schemas (3 core tools)
* **Enhanced Format Discovery**: Real-time format detection with human-readable descriptions
* **SPARQL Execution**: Working query execution with multiple output formats
* **Query Validation**: Syntax checking, query type detection, and variable extraction for SELECT * queries
* **Error Reporting**: Comprehensive error handling and reporting with proper request ID matching
* **Command-Line Options**: Full CLI with help, version, and debugging support
* **File Logging**: Persistent logging with structured output and timestamps
* **MCP Compliance**: Full Model Context Protocol specification compliance
* **Code Quality**: Clean, maintainable C code following best practices

## Benefits

1. **AI-Driven SPARQL Development**: Enable AI assistants to help write, validate, and test SPARQL queries
2. **Automated Testing Integration**: Seamless integration with AI-powered testing workflows
3. **Educational Tool**: AI tutors could use this to teach SPARQL interactively
4. **Quality Assurance**: Automated query validation in CI/CD pipelines via AI agents
5. **Rapid Prototyping**: Quick SPARQL experimentation with immediate feedback
6. **Rich Format Information**: Detailed descriptions help users choose appropriate formats
7. **Professional Output**: Enhanced tool responses provide better user experience

## Troubleshooting

### Common Issues

1. **Build failures**: Ensure YAJL is available and Rasqal is built with JSON support
2. **Runtime errors**: Check that data files exist and are readable
3. **Format errors**: Verify that input/output formats are supported

### Debug Mode

Compile Rasqal with `--enable-debug` on configure

You can also use the built-in debug option:

```bash
./utils/rasqal-mcp-server --debug
```

### Logging

Enable file logging for persistent debugging:

```bash
./utils/rasqal-mcp-server --log-file debug.log
```
