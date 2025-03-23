import {
    InitializedEvent,
    LoggingDebugSession,
    StoppedEvent,
    Thread
} from "@vscode/debugadapter";
import { DebugProtocol } from "@vscode/debugprotocol";

const mainThreadId: number = 1;

export class AsdbgSession extends LoggingDebugSession {
    private breakpoints: Map<string, DebugProtocol.SourceBreakpoint[]> = new Map();

    public constructor(fileAccessor: any) {
        super('angel-debug.txt', fileAccessor);

        // Here it is 1-index
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
    }

    protected attachRequest(response: DebugProtocol.AttachResponse, args: DebugProtocol.AttachRequestArguments, request?: DebugProtocol.Request) {
        super.attachRequest(response, args, request);
        this.sendResponse(response);
    }

    protected async setBreakPointsRequest(response: DebugProtocol.SetBreakpointsResponse, args: DebugProtocol.SetBreakpointsArguments, request?: DebugProtocol.Request) {
        console.log('Set breakpoints request received.');
        super.setBreakPointsRequest(response, args, request);

        if (args.source.path !== undefined && args.breakpoints !== undefined) {
            this.breakpoints.set(args.source.path, args.breakpoints);
        }

        // TODO: Implement breakpoint sender
        this.dummyProcess().catch(console.error);

        this.sendResponse(response);
    }

    private async dummyProcess() {
        await new Promise((resolve) => setTimeout(resolve, 1000));

        this.sendEvent(new StoppedEvent('止まります', mainThreadId));
    }

    protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
        response.body = { threads: [new Thread(mainThreadId, "Thread"),] };
        this.sendResponse(response);
    }

    protected breakpointLocationsRequest(response: DebugProtocol.BreakpointLocationsResponse, args: DebugProtocol.BreakpointLocationsArguments, request?: DebugProtocol.Request): void {
        // このファイルのどこにブレークポイントを設定できるかを返す
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
        const filename = [...this.breakpoints.keys()][0];
        response.body = {
            stackFrames: [
                {
                    id: 1,
                    name: filename,
                    line: 12,
                    column: 1,
                    source: {
                        name: 'name=' + filename,
                        path: filename
                    }
                }
            ]
        };

        this.sendResponse(response);
    }
}
