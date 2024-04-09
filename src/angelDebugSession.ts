import {
    InitializedEvent,
    LoggingDebugSession,
    StoppedEvent,
    Thread
} from "@vscode/debugadapter";
import {DebugProtocol} from "@vscode/debugprotocol";

const thread_1: number = 1;

export class AngelDebugSession extends LoggingDebugSession {
    private breakpoints: Map<string, DebugProtocol.SourceBreakpoint[]> = new Map();

    public constructor(fileAccessor: any) {
        super('angel-debug.txt', fileAccessor);

        // 1-index で行列を扱う
        this.setDebuggerLinesStartAt1(true);
        this.setDebuggerColumnsStartAt1(true);
    }

    protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments) {
        super.initializeRequest(response, args);

        response.body = response.body || {};

        response.body.supportsConfigurationDoneRequest = true;
        response.body.supportsEvaluateForHovers = true;
        response.body.supportsStepBack = true;
        response.body.supportsDataBreakpoints = true;
        response.body.supportsCompletionsRequest = true;
        response.body.completionTriggerCharacters = [".", "["];
        response.body.supportsCancelRequest = true;
        response.body.supportsBreakpointLocationsRequest = true;
        response.body.supportsStepInTargetsRequest = true;

        response.body.supportSuspendDebuggee = true;
        response.body.supportTerminateDebuggee = true;
        response.body.supportsFunctionBreakpoints = true;
        response.body.supportsDelayedStackTraceLoading = true;

        this.startProcess().catch(console.error);

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

        this.sendResponse(response);
    }

    private async startProcess() {
        await new Promise((resolve) => setTimeout(resolve, 1000));

        this.sendEvent(new StoppedEvent('止まります', thread_1));
    }

    protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
        response.body = {threads: [new Thread(thread_1, "Thread"),]};
        this.sendResponse(response);
    }

    protected async stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments, request?: DebugProtocol.Request) {
        const p = [...this.breakpoints.keys()][0];
        response.body = {
            stackFrames: [
                {
                    id: 1,
                    name: p,
                    line: 1,
                    column: 1,
                    source: {
                        name: p,
                        path: p
                    }
                }
            ]
        };
        this.sendResponse(response);
    }
}
