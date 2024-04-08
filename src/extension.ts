import * as vscode from 'vscode';
import {activateDebugger} from './activateDebugger';

/*
 * The compile time flag 'runMode' controls how the debug adapter is run.
 * Please note: the test suite only supports 'external' mode.
 */


export function activate(context: vscode.ExtensionContext) {
    // run the debug adapter inside the extension and directly talk to it
    activateDebugger(context);
}

export function deactivate() {
    // nothing to do
}
