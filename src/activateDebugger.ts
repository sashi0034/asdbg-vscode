import * as vscode from 'vscode';
import { WorkspaceFolder, DebugConfiguration, ProviderResult, CancellationToken } from 'vscode';
import { FileAccessor } from './mockRuntime';
import { AngelDebugSession } from "./angelDebugSession";
import { MockDebugSession } from "./mockDebug";

export function activateDebugger(context: vscode.ExtensionContext) {
    context.subscriptions.push(
        vscode.commands.registerCommand('asdbg-vscode.debugEditorContents', (resource: vscode.Uri) => {
            let targetResource = resource;
            if (!targetResource && vscode.window.activeTextEditor) {
                targetResource = vscode.window.activeTextEditor.document.uri;
            }
            if (targetResource) {
                vscode.debug.startDebugging(undefined, {
                    type: 'asdbg',
                    name: 'Debug File',
                    request: 'attach',
                    program: targetResource.fsPath,
                    stopOnEntry: true
                });
            }
        }),
        vscode.commands.registerCommand('asdbg-vscode.toggleFormatting', (variable) => {
            const ds = vscode.debug.activeDebugSession;
            if (ds) {
                ds.customRequest('toggleFormatting');
            }
        })
    );

    context.subscriptions.push(vscode.commands.registerCommand('asdbg-vscode.getProgramName', config => {
        return vscode.window.showInputBox({
            placeHolder: "Please enter the name of a markdown file in the workspace folder",
            value: "readme.md"
        });
    }));

    // register a configuration provider for 'asdbg' debug type
    const provider = new AsdbgConfigurationProvider();
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('asdbg', provider));

    const factory = new InlineDebugAdapterFactory();
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('asdbg', factory));

    vscode.debug.startDebugging(undefined, {
        type: 'asdbg',
        name: 'Debug File',
        request: 'attach',
        stopOnEntry: true
    });

    // override VS Code's default implementation of the debug hover

    context.subscriptions.push(vscode.languages.registerEvaluatableExpressionProvider('angelscript', {
        provideEvaluatableExpression(document: vscode.TextDocument, position: vscode.Position): vscode.ProviderResult<vscode.EvaluatableExpression> {

            const variableRegexp = /(?<![\w$])(?![0-9])[\w$]+/g;
            const line = document.lineAt(position.line).text;

            let m: RegExpExecArray | null;
            while (m = variableRegexp.exec(line)) {
                const varRange = new vscode.Range(position.line, m.index, position.line, m.index + m[0].length);

                if (varRange.contains(position)) {
                    return new vscode.EvaluatableExpression(varRange);
                }
            }
            return undefined;
        }
    }));

    // override VS Code's default implementation of the "inline values" feature"
    context.subscriptions.push(vscode.languages.registerInlineValuesProvider('angelscript', {

        provideInlineValues(document: vscode.TextDocument, viewport: vscode.Range, context: vscode.InlineValueContext): vscode.ProviderResult<vscode.InlineValue[]> {

            const allValues: vscode.InlineValue[] = [];

            for (let l = viewport.start.line; l <= context.stoppedLocation.end.line; l++) {
                const line = document.lineAt(l);
                const regExp = /(?<![\w$])(?![0-9])[\w$]+/g;
                let m: RegExpExecArray | null;
                do {
                    m = regExp.exec(line.text);
                    if (m) {
                        const varName = m[1];
                        const varRange = new vscode.Range(l, m.index, l, m.index + varName.length);

                        // some literal text
                        //allValues.push(new vscode.InlineValueText(varRange, `${varName}: ${viewport.start.line}`));

                        // value found via variable lookup
                        allValues.push(new vscode.InlineValueVariableLookup(varRange, varName, false));

                        // value determined via expression evaluation
                        //allValues.push(new vscode.InlineValueEvaluatableExpression(varRange, varName));
                    }
                } while (m);
            }

            return allValues;
        }
    }));
}

class AsdbgConfigurationProvider implements vscode.DebugConfigurationProvider {

    /**
     * Massage a debug configuration just before a debug session is being launched,
     * e.g. add all missing attributes to the debug configuration.
     */
    resolveDebugConfiguration(folder: WorkspaceFolder | undefined, config: DebugConfiguration, token?: CancellationToken): ProviderResult<DebugConfiguration> {

        // If launch.json is missing or empty
        if (!config.type && !config.request && !config.name) {
            const editor = vscode.window.activeTextEditor;
            if (editor && editor.document.languageId === 'angelscript') {
                config.type = 'asdbg';
                config.name = 'Attach';
                config.request = 'attach';
                config.program = '${file}';
                config.stopOnEntry = true;
            }
        }

        // if (!config.program) {
        //     return vscode.window.showInformationMessage("Cannot find a program to debug").then(_ => {
        //         return undefined;	// Abort launch
        //     });
        // }

        return config;
    }
}

export const workspaceFileAccessor: FileAccessor = {
    isWindows: typeof process !== 'undefined' && process.platform === 'win32',
    async readFile(path: string): Promise<Uint8Array> {
        let uri: vscode.Uri;
        try {
            uri = pathToUri(path);
        } catch (e) {
            return new TextEncoder().encode(`cannot read '${path}'`);
        }

        return await vscode.workspace.fs.readFile(uri);
    },
    async writeFile(path: string, contents: Uint8Array) {
        await vscode.workspace.fs.writeFile(pathToUri(path), contents);
    }
};

function pathToUri(path: string) {
    try {
        return vscode.Uri.file(path);
    } catch (e) {
        return vscode.Uri.parse(path);
    }
}

class InlineDebugAdapterFactory implements vscode.DebugAdapterDescriptorFactory {

    createDebugAdapterDescriptor(_session: vscode.DebugSession): ProviderResult<vscode.DebugAdapterDescriptor> {
        return new vscode.DebugAdapterInlineImplementation(new AngelDebugSession(workspaceFileAccessor));
    }

    // a() {
    //     return new AngelDebugSession(workspaceFileAccessor);
    // }

    // d() {
    //     return new MockDebugSession(workspaceFileAccessor);
    // }
}
