import {
    InitializedEvent,
    LoggingDebugSession,
    StoppedEvent,
    Thread
} from "@vscode/debugadapter";
import { DebugProtocol } from "@vscode/debugprotocol";
import * as net from 'net';

// クライアントソケットを保持する配列
const s_clients: net.Socket[] = [];

const mainThreadId: number = 1;

export class AsdbgSession extends LoggingDebugSession {
    // ブレイクポイントはファイルパスごとに配列で保持する
    public breakpoints: Map<string, DebugProtocol.SourceBreakpoint[]> = new Map();

    private _currentLine: number = 1;
    private _currentFile: string | undefined = undefined;

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
            s_clients.push(socket);
            console.log('Client connected');

            // クライアントからのデータ受信時
            socket.on('data', (data: Buffer) => {
                const msg = data.toString().trim();
                console.log(`Client says: ${msg}`);

                if (msg === 'PING') {
                    socket.write('PONG\n');
                } else if (msg === 'HELLO') {
                    socket.write('Hello, client!\n');
                } else if (msg === 'GET_BREAKPOINTS') {
                    // ブレイクポイント要求時、現在のブレイクポイント情報を送信
                    this.sendBreakpoints(socket);
                } else {
                    socket.write('Unknown command\n');
                }
            });

            socket.on('end', () => {
                console.log('Client disconnected');
                // 切断したソケットを配列から削除
                const index = s_clients.indexOf(socket);
                if (index !== -1) {
                    s_clients.splice(index, 1);
                }
            });
        });

        server.listen(4712, () => {
            console.log('Server listening on port 4712');
        });
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

        // TODO: ブレイクポイント設定後の処理（例: ダミープロセスで一時停止）
        this.dummyProcess().catch(console.error);

        // ブレイクポイント更新を接続中の全クライアントへ送信
        for (const socket of s_clients) {
            this.sendBreakpoints(socket);
        }

        this.sendResponse(response);
    }

    private async dummyProcess() {
        await new Promise((resolve) => setTimeout(resolve, 1000));

        // 最初のブレイクポイントの位置に移動
        const bpEntry = [...this.breakpoints.entries()][0];
        if (bpEntry) {
            this._currentFile = bpEntry[0];
            const firstBp = bpEntry[1][0];
            this._currentLine = firstBp.line;
        }

        this.sendEvent(new StoppedEvent('breakpoint', mainThreadId));
    }

    protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
        response.body = { threads: [new Thread(mainThreadId, "Thread")] };
        this.sendResponse(response);
    }

    protected breakpointLocationsRequest(response: DebugProtocol.BreakpointLocationsResponse, args: DebugProtocol.BreakpointLocationsArguments, request?: DebugProtocol.Request): void {
        // このファイル内でブレイクポイントを設定できる箇所
        response.body = {
            breakpoints: [
                {
                    line: 4,
                    column: 1,
                    endLine: 4,
                    endColumn: 1
                }
            ]
        };

        this.sendResponse(response);
    }

    protected async stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments, request?: DebugProtocol.Request) {
        response.body = {
            stackFrames: [
                {
                    id: 1,
                    name: this._currentFile ?? 'unknown',
                    line: this._currentLine,
                    column: 1,
                    source: {
                        name: this._currentFile ?? 'unknown',
                        path: this._currentFile
                    }
                }
            ]
        };
        this.sendResponse(response);
    }

    protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
        this._currentLine++;
        this.sendEvent(new StoppedEvent('step', mainThreadId));
        this.sendResponse(response);
    }

    protected stepInRequest(response: DebugProtocol.StepInResponse, args: DebugProtocol.StepInArguments): void {
        this._currentLine++;
        this.sendEvent(new StoppedEvent('step', mainThreadId));
        this.sendResponse(response);
    }

    protected continueRequest(response: DebugProtocol.ContinueResponse, args: DebugProtocol.ContinueArguments): void {
        this.sendResponse(response);
    }
}
