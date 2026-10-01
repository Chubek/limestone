#!/usr/bin/env node
'use strict';

// Dependency-free Language Server Protocol adapter for Satie CNF/DIMACS files.
const input = process.stdin;
const output = process.stdout;
let pending = Buffer.alloc(0);
let expected = -1;
const documents = new Map();

function send(message) {
  const body = Buffer.from(JSON.stringify(message), 'utf8');
  output.write(`Content-Length: ${body.length}\r\n\r\n`);
  output.write(body);
}

function respond(id, result) { send({jsonrpc: '2.0', id, result}); }
function diagnostic(line, column, end, message) {
  return {range: {start: {line, character: column}, end: {line, character: end}},
          severity: 1, source: 'satie', message};
}

function validate(text) {
  const lines = text.split(/\r?\n/);
  const result = [];
  const first = lines.find(line => line.trim() && !/^\s*c(?:\s|$)/.test(line));
  if (/^\s*p\s+cnf(?:\s|$)/.test(first || '')) {
    let declared = null;
    let count = 0;
    let terminated = true;
    lines.forEach((line, row) => {
      if (!line.trim() || /^\s*c(?:\s|$)/.test(line)) return;
      if (/^\s*p(?:\s|$)/.test(line)) {
        const match = /^\s*p\s+cnf\s+(\d+)\s+(\d+)\s*$/.exec(line);
        if (!match || declared !== null) result.push(diagnostic(row, 0, line.length, 'Invalid or duplicate DIMACS header'));
        else declared = {variables: Number(match[1]), clauses: Number(match[2])};
        return;
      }
      for (const match of line.matchAll(/\S+/g)) {
        const token = match[0];
        if (!/^-?\d+$/.test(token) || token === '-0') {
          result.push(diagnostic(row, match.index, match.index + token.length, 'Expected an integer literal'));
          continue;
        }
        const value = Number(token);
        if (value === 0) { count++; terminated = true; }
        else {
          terminated = false;
          if (declared && Math.abs(value) > declared.variables)
            result.push(diagnostic(row, match.index, match.index + token.length, 'Variable exceeds DIMACS header count'));
        }
      }
    });
    if (!terminated) result.push(diagnostic(lines.length - 1, 0, lines.at(-1).length, 'Clause must end with 0'));
    if (declared && count !== declared.clauses)
      result.push(diagnostic(0, 0, lines[0].length, `Expected ${declared.clauses} clauses, found ${count}`));
    return result;
  }
  const stack = [];
  lines.forEach((line, row) => {
    const comment = line.indexOf('#');
    const code = comment < 0 ? line : line.slice(0, comment);
    for (let column = 0; column < code.length; ++column) {
      if (code[column] === '(') stack.push({row, column});
      if (code[column] === ')') {
        if (stack.length) stack.pop();
        else result.push(diagnostic(row, column, column + 1, 'Unexpected closing parenthesis'));
      }
    }
  });
  stack.forEach(({row, column}) => result.push(diagnostic(row, column, column + 1, 'Unclosed parenthesis')));
  return result;
}

function publish(uri, text, version) {
  send({jsonrpc: '2.0', method: 'textDocument/publishDiagnostics',
        params: {uri, version, diagnostics: validate(text)}});
}

function handle(message) {
  const {id, method, params = {}} = message;
  if (method === 'initialize') {
    respond(id, {capabilities: {textDocumentSync: 1,
      completionProvider: {triggerCharacters: [':']}, hoverProvider: true},
      serverInfo: {name: 'satie-lsp', version: '0.1.0'}});
  } else if (method === 'shutdown') {
    respond(id, null);
  } else if (method === 'exit') {
    process.exitCode = 0;
    input.pause();
  } else if (method === 'textDocument/didOpen') {
    const document = params.textDocument;
    documents.set(document.uri, document.text);
    publish(document.uri, document.text, document.version);
  } else if (method === 'textDocument/didChange') {
    const document = params.textDocument;
    const change = params.contentChanges?.at(-1);
    if (change && typeof change.text === 'string') {
      documents.set(document.uri, change.text);
      publish(document.uri, change.text, document.version);
    }
  } else if (method === 'textDocument/didClose') {
    documents.delete(params.textDocument.uri);
    publish(params.textDocument.uri, '', undefined);
  } else if (method === 'textDocument/completion') {
    respond(id, {isIncomplete: false, items: [':load', ':solve', ':engine', ':help',
      'native', 'dpll', 'cdcl'].map(label => ({label, kind: 14}))});
  } else if (method === 'textDocument/hover') {
    const {uri} = params.textDocument;
    const line = documents.get(uri)?.split(/\r?\n/)[params.position.line] || '';
    const column = params.position.character;
    const before = line.slice(0, column).match(/[\w:]+$/)?.[0] || '';
    const after = line.slice(column).match(/^\w*/)?.[0] || '';
    const term = before + after;
    const help = {native: 'Exhaustive SAT solver', dpll: 'DPLL SAT solver',
      cdcl: 'Clause learning SAT solver', ':solve': 'Solve the current formula',
      ':load': 'Load a formula from a file'};
    respond(id, help[term] ? {contents: {kind: 'plaintext', value: help[term]}} : null);
  } else if (id !== undefined) {
    respond(id, null);
  }
}

input.on('data', chunk => {
  pending = Buffer.concat([pending, chunk]);
  while (true) {
    if (expected < 0) {
      const end = pending.indexOf('\r\n\r\n');
      if (end < 0) return;
      const header = pending.subarray(0, end).toString('ascii');
      const length = /^Content-Length:\s*(\d+)\s*$/im.exec(header);
      pending = pending.subarray(end + 4);
      if (!length || Number(length[1]) > 8 * 1024 * 1024) { input.pause(); return; }
      expected = Number(length[1]);
    }
    if (pending.length < expected) return;
    const body = pending.subarray(0, expected);
    pending = pending.subarray(expected);
    expected = -1;
    try { handle(JSON.parse(body.toString('utf8'))); }
    catch (error) { process.stderr.write(`satie-lsp: ${error.message}\n`); }
  }
});
