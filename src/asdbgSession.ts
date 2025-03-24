import {
    InitializedEvent,
    LoggingDebugSession,
    StoppedEvent,
    Thread
} from "@vscode/debugadapter";
import { DebugProtocol } from "@vscode/debugprotocol";
import * as net from 'net';

const mainThreadId = 1;

interface ScriptBreakpoint {
    filepath: string;
    line: number;
}

interface ScriptVariable {
    name: string;
    value: string;
}

export class AsdbgSession extends LoggingDebugSession {
    // ブレイクポイントはファイルパスごとに配列で保持する
    public breakpoints: Map<string, DebugProtocol.SourceBreakpoint[]> = new Map();

    private readonly _clients: net.Socket[] = [];

    private _currentBreakpoint: ScriptBreakpoint | undefined;

    private readonly _variables: ScriptVariable[] = [];

    public constructor(fileAccessor: any) {
        super('angel-debug.txt', fileAccessor);

        // 1-index（行番号、カラム番号）に合わせる
        this.setDebuggerLinesStartAt1(true);
        this.setDebuggerColumnsStartAt1(true);
    }

    protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments) {
        super.initializeRequest(response, args);

        response.body = response.body || {};

        // FIXME
        // 構成完了通知（configurationDoneRequest）をサポートする
        response.body.supportsConfigurationDoneRequest = true;

        // ホバー表示中に evaluateRequest を使用して式の評価ができる
        response.body.supportsEvaluateForHovers = true;

        // ステップバック（逆方向ステップ実行）をサポートする
        response.body.supportsStepBack = true;

        // データブレークポイント（変数の値の変更など）をサポートする
        response.body.supportsDataBreakpoints = true;

        // コード補完（completionsRequest）をサポートする
        response.body.supportsCompletionsRequest = true;

        // コード補完をトリガーする文字（例: ".", "["）
        response.body.completionTriggerCharacters = [".", "["];

        // 実行中のリクエストをキャンセルできる（supportsCancelRequest）
        response.body.supportsCancelRequest = true;

        // ブレークポイントが有効かどうかを問い合わせるためのリクエスト（breakpointLocationsRequest）をサポートする
        response.body.supportsBreakpointLocationsRequest = true;

        // ステップインのターゲット（関数呼び出しのどれに入るか）を指定できる
        response.body.supportsStepInTargetsRequest = true;

        // デバッガーがデバッグ対象プログラムの一時停止をサポートする（pause など）
        response.body.supportSuspendDebuggee = true;

        // デバッガーがデバッグ対象プログラムの終了をサポートする（terminate など）
        response.body.supportTerminateDebuggee = true;

        // 関数ブレークポイント（関数名に基づくブレーク）をサポートする
        response.body.supportsFunctionBreakpoints = true;

        // スタックトレースの遅延読み込み（必要になった時に取得）をサポートする
        response.body.supportsDelayedStackTraceLoading = true;

        response.body.supportSuspendDebuggee = true;
        response.body.supportTerminateDebuggee = true;
        response.body.supportsFunctionBreakpoints = true;
        response.body.supportsDelayedStackTraceLoading = true;

        this.sendResponse(response);
        this.sendEvent(new InitializedEvent());

        // ネットワークサーバの起動（ポート4712）
        const server = net.createServer((socket: net.Socket) => {
            // 接続時、クライアントソケットを配列に追加
            this._clients.push(socket);
            console.log('Client connected');

            // クライアントからのデータ受信時
            socket.on('data', (data: Buffer) => {
                const messages = data.toString().split('\n');
                while (messages.length > 0) {
                    this.handleSocketData(socket, messages);
                }
            });

            socket.on('end', () => {
                console.log('Client disconnected');

                // 切断したソケットを配列から削除
                const index = this._clients.indexOf(socket);
                if (index !== -1) {
                    this._clients.splice(index, 1);
                }
            });
        });

        server.listen(4712, () => {
            console.log('Server listening on port 4712');
        });

        console.log('Debug adapter initialized!');
    }

    private handleSocketData(socket: net.Socket, messages: string[]) {
        const method = messages.shift();
        if (method === undefined || method === '') {
            return;
        }
        else if (method === 'GET_BREAKPOINTS') {
            // ブレイクポイント要求時、現在のブレイクポイント情報を送信
            this.sendBreakpoints(socket);
        } else if (method === 'STOP') {
            // ```
            // STOP
            // filepath,line
            // ```
            const nextMessage = messages.shift();
            const [filepath, line] = nextMessage ? nextMessage.split(',') : [undefined, undefined];
            const lineNumber = line !== undefined ? parseInt(line, 10) : undefined;
            if (filepath === undefined || lineNumber === undefined) {
                console.log('Invalid STOP message received.');
                return;
            }

            this._currentBreakpoint = {
                filepath: filepath,
                line: lineNumber
            };

            // Send message for VSCode to stop at the breakpoint
            this.sendEvent(new StoppedEvent('breakpoint', mainThreadId));
        }
        else if (method === 'VARIABLES') {
            // ```
            // VARIABLES
            // 2
            // variable_name_1
            // 123
            // variable_name_2
            // 456
            // ```
            this._variables.length = 0; // clear
            const count = parseInt(messages.shift() ?? '0', 10);
            for (let i = 0; i < count; i++) {
                const name = messages.shift();
                const value = messages.shift();
                if (name === undefined || value === undefined) {
                    console.log('Invalid VARIABLES message received.');
                    break;
                }

                this._variables.push({
                    name: name,
                    value: value
                });
            }
        } else {
            console.log('Unknown message received: ' + method);
        }
    }

    // ブレイクポイント情報を送信するヘルパー
    private sendBreakpoints(socket: net.Socket): void {
        let message = "BREAKPOINTS\n";
        for (const [filepath, bps] of this.breakpoints.entries()) {
            for (const bp of bps) {
                // message += `${filepath},${bp.line},${bp.column}\n`;
                message += `${filepath},${bp.line}\n`;
            }
        }

        message += "END_BREAKPOINTS\n";
        socket.write(message);
        console.log('Sent breakpoints to client:\n' + message);
    }

    protected attachRequest(response: DebugProtocol.AttachResponse, args: DebugProtocol.AttachRequestArguments, request?: DebugProtocol.Request) {
        super.attachRequest(response, args, request);
        this.sendResponse(response);
    }

    protected async setBreakPointsRequest(response: DebugProtocol.SetBreakpointsResponse, args: DebugProtocol.SetBreakpointsArguments, request?: DebugProtocol.Request) {
        if (args.source.path?.endsWith('.as') === false) {
            return;
        }

        console.log('Set breakpoints request received.');
        super.setBreakPointsRequest(response, args, request);

        if (args.source.path !== undefined && args.breakpoints !== undefined) {
            this.breakpoints.set(args.source.path, args.breakpoints);
        }

        // ブレイクポイント更新を接続中の全クライアントへ送信
        for (const socket of this._clients) {
            this.sendBreakpoints(socket);
        }

        this.sendResponse(response);
    }

    protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
        response.body = { threads: [new Thread(mainThreadId, "Thread")] };
        this.sendResponse(response);
    }

    // protected breakpointLocationsRequest(response: DebugProtocol.BreakpointLocationsResponse, args: DebugProtocol.BreakpointLocationsArguments, request?: DebugProtocol.Request): void {
    //     // このファイル内でブレイクポイントを設定できる箇所
    //     response.body = {
    //         breakpoints: [
    //             {
    //                 line: 4,
    //                 column: 1,
    //                 endLine: 4,
    //                 endColumn: 1
    //             }
    //         ]
    //     };

    //     this.sendResponse(response);
    // }

    // VSCode がスタックトレースを要求したときに呼び出される
    protected async stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments, request?: DebugProtocol.Request) {
        const filepath = this._currentBreakpoint?.filepath ?? 'unknown';
        response.body = {
            stackFrames: [
                {
                    id: 1,
                    name: filepath,
                    line: this._currentBreakpoint?.line ?? 0,
                    column: 1,
                    source: {
                        name: filepath,
                        path: filepath
                    }
                }
            ]
        };

        this.sendResponse(response);
    }

    protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
        this._clients.forEach(client => {
            client.write('COMMAND\nSTEP_OVER\n');
        });

        this.sendResponse(response);
    }

    protected stepInRequest(response: DebugProtocol.StepInResponse, args: DebugProtocol.StepInArguments): void {
        this._clients.forEach(client => {
            client.write('COMMAND\nSTEP_IN\n');
        });

        this.sendResponse(response);
    }

    protected continueRequest(response: DebugProtocol.ContinueResponse, args: DebugProtocol.ContinueArguments): void {
        this._clients.forEach(client => {
            client.write('COMMAND\nCONTINUE\n');
        });

        this.sendResponse(response);
    }

    // protected evaluateRequest(response: DebugProtocol.EvaluateResponse, args: DebugProtocol.EvaluateArguments, request?: DebugProtocol.Request): void {

    // }

    protected scopesRequest(response: DebugProtocol.ScopesResponse, args: DebugProtocol.ScopesArguments, request?: DebugProtocol.Request): void {
        response.body = {
            scopes: [
                {
                    name: "my_function",
                    variablesReference: 1000, // FIXME
                    expensive: false
                }
            ]
        };

        this.sendResponse(response);
    }

    protected variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments, request?: DebugProtocol.Request): void {
        response.body = {
            variables: this._variables.map((v, i) => {
                return {
                    name: v.name,
                    value: v.value,
                    variablesReference: 2000 + i // FIXME
                };
            })
        };

        this.sendResponse(response);
    }
}
