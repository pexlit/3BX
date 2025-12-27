import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    StreamInfo,
    State
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

// Store extension context for later use
let extensionContext: vscode.ExtensionContext | undefined;

// Global output channel for extension logging
let outputChannel: vscode.OutputChannel | undefined;

/**
 * Logs a message to the output channel with timestamp.
 * Creates the output channel if it does not exist.
 */
function log(message: string, level: 'INFO' | 'WARN' | 'ERROR' | 'DEBUG' = 'INFO'): void {
    if (!outputChannel) {
        outputChannel = vscode.window.createOutputChannel('3BX Extension');
    }
    const timestamp = new Date().toISOString();
    outputChannel.appendLine(`[${timestamp}] [${level}] ${message}`);

    // Also log to console for Developer Tools access
    const consoleMessage = `[3BX] [${level}] ${message}`;
    if (level === 'ERROR') {
        console.error(consoleMessage);
    } else if (level === 'WARN') {
        console.warn(consoleMessage);
    } else if (level === 'DEBUG') {
        console.debug(consoleMessage);
    } else {
        console.log(consoleMessage);
    }
}

/**
 * Resolves VS Code variables like ${workspaceFolder} in a string.
 * VS Code does not automatically substitute these in settings.json values.
 */
function resolveVariables(value: string): string {
    // Resolve ${workspaceFolder}
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    if (workspaceFolder) {
        value = value.replace(/\$\{workspaceFolder\}/g, workspaceFolder);
    }

    // Resolve ${userHome}
    const userHome = process.env.HOME || process.env.USERPROFILE || '';
    value = value.replace(/\$\{userHome\}/g, userHome);

    // Resolve environment variables ${env:VAR_NAME}
    value = value.replace(/\$\{env:([^}]+)\}/g, (_, varName) => {
        return process.env[varName] || '';
    });

    return value;
}

/**
 * Activates the 3BX language extension.
 * Sets up the Language Server Protocol client and Debug Adapter.
 */
export async function activate(context: vscode.ExtensionContext): Promise<void> {
    // Initialize logging immediately
    log('========================================');
    log('3BX Extension: Activation started');
    log(`Extension path: ${context.extensionPath}`);
    log(`Extension mode: ${context.extensionMode === vscode.ExtensionMode.Development ? 'Development' : context.extensionMode === vscode.ExtensionMode.Test ? 'Test' : 'Production'}`);
    log(`VS Code version: ${vscode.version}`);
    log(`Platform: ${process.platform}`);
    log(`Node version: ${process.version}`);

    extensionContext = context;

    // Show output channel so user can see logs
    if (outputChannel) {
        outputChannel.show(true);  // true = preserve focus
    }

    // Register commands
    log('Registering commands...');
    context.subscriptions.push(
        vscode.commands.registerCommand('3bx.restartServer', restartServer),
        vscode.commands.registerCommand('3bx.compileFile', compileCurrentFile),
        vscode.commands.registerCommand('3bx.showLogs', () => {
            if (outputChannel) {
                outputChannel.show();
            }
        })
    );
    log('Commands registered: 3bx.restartServer, 3bx.compileFile, 3bx.showLogs');

    // Register debug adapter
    log('Registering debug adapter...');
    registerDebugAdapter(context);
    log('Debug adapter registered');

    // Start LSP client if enabled
    const config = vscode.workspace.getConfiguration('3bx');
    const lspEnabled = config.get<boolean>('lsp.enabled', true);
    log(`Configuration: lsp.enabled = ${lspEnabled}`);
    log(`Configuration: compiler.path = "${config.get<string>('compiler.path', '')}"`);
    log(`Configuration: lsp.trace.server = "${config.get<string>('lsp.trace.server', 'off')}"`);

    if (lspEnabled) {
        log('LSP is enabled, starting language client...');
        await startLanguageClient(context);
    } else {
        log('LSP is disabled in settings, skipping language client start');
    }

    // Watch for configuration changes
    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(async (e) => {
            if (e.affectsConfiguration('3bx.lsp.enabled') ||
                e.affectsConfiguration('3bx.compiler.path')) {
                log('Configuration changed, restarting server...');
                await restartServer();
            }
        })
    );

    log('3BX Extension: Activation completed successfully');
    log('========================================');

    // Show a status bar item to indicate activation status
    const statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBarItem.text = '$(check) 3BX';
    statusBarItem.tooltip = '3BX Extension Active - Click to show logs';
    statusBarItem.command = '3bx.showLogs';
    statusBarItem.show();
    context.subscriptions.push(statusBarItem);

    // Show a prominent notification that the extension has activated
    vscode.window.showInformationMessage(
        '3BX Extension activated successfully!',
        'Show Logs'
    ).then(selection => {
        if (selection === 'Show Logs' && outputChannel) {
            outputChannel.show();
        }
    });
}

/**
 * Deactivates the extension and stops the language client.
 */
export async function deactivate(): Promise<void> {
    log('3BX Extension: Deactivating...');
    if (client) {
        log('  Stopping language client...');
        await client.stop();
        client = undefined;
        log('  Language client stopped');
    }
    log('3BX Extension: Deactivated');
}

/**
 * Registers the debug adapter for 3BX debugging.
 */
function registerDebugAdapter(context: vscode.ExtensionContext): void {
    // Register configuration provider
    const configProvider = new ThreeBXConfigurationProvider();
    context.subscriptions.push(
        vscode.debug.registerDebugConfigurationProvider('3bx', configProvider)
    );

    // Register debug adapter descriptor factory
    const factory = new ThreeBXDebugAdapterFactory();
    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('3bx', factory)
    );
}

/**
 * Debug configuration provider for 3BX.
 * Provides initial debug configurations and resolves configuration before debugging.
 */
class ThreeBXConfigurationProvider implements vscode.DebugConfigurationProvider {
    /**
     * Provides initial debug configurations when creating launch.json
     */
    provideDebugConfigurations(
        folder: vscode.WorkspaceFolder | undefined,
        token?: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.DebugConfiguration[]> {
        return [
            {
                type: '3bx',
                request: 'launch',
                name: 'Debug 3BX Program',
                program: '${file}',
                stopOnEntry: false
            }
        ];
    }

    /**
     * Resolves and validates the debug configuration before starting debug session
     */
    resolveDebugConfiguration(
        folder: vscode.WorkspaceFolder | undefined,
        config: vscode.DebugConfiguration,
        token?: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.DebugConfiguration> {
        // If no configuration provided, use defaults
        if (!config.type && !config.request && !config.name) {
            const editor = vscode.window.activeTextEditor;
            if (editor && editor.document.languageId === '3bx') {
                config.type = '3bx';
                config.request = 'launch';
                config.name = 'Debug 3BX Program';
                config.program = editor.document.uri.fsPath;
                config.stopOnEntry = false;
            }
        }

        // Ensure program is specified
        if (!config.program) {
            return vscode.window.showInformationMessage(
                'Cannot debug: no program specified'
            ).then(_ => undefined);
        }

        return config;
    }

    /**
     * Resolves the debug configuration after all variables have been substituted
     */
    resolveDebugConfigurationWithSubstitutedVariables(
        folder: vscode.WorkspaceFolder | undefined,
        config: vscode.DebugConfiguration,
        token?: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.DebugConfiguration> {
        return config;
    }
}

/**
 * Factory for creating debug adapter descriptors.
 * Creates an executable debug adapter using the 3BX compiler with --dap flag.
 */
class ThreeBXDebugAdapterFactory implements vscode.DebugAdapterDescriptorFactory {
    createDebugAdapterDescriptor(
        session: vscode.DebugSession,
        executable: vscode.DebugAdapterExecutable | undefined
    ): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {
        const config = vscode.workspace.getConfiguration('3bx');
        let compilerPath = config.get<string>('compiler.path', '');

        if (!compilerPath) {
            compilerPath = 'threebx';  // Use from PATH
        } else {
            // Resolve VS Code variables that aren't automatically substituted in settings
            compilerPath = resolveVariables(compilerPath);
        }

        const args = ['--dap'];

        // Add debug flag if configured
        if (config.get<boolean>('debug.verbose', false)) {
            args.push('--debug');
        }

        return new vscode.DebugAdapterExecutable(compilerPath, args);
    }
}

/**
 * Starts the Language Server Protocol client.
 * Spawns the 3BX compiler with --lsp flag as the server.
 */
async function startLanguageClient(context: vscode.ExtensionContext): Promise<void> {
    const config = vscode.workspace.getConfiguration('3bx');
    const lspOutputChannel = vscode.window.createOutputChannel('3BX Language Server');
    const traceLevel = config.get<string>('lsp.trace.server', 'off');

    log('Starting Language Server Protocol client...');
    log(`  Trace level: ${traceLevel}`);

    // Get compiler path from settings or use default
    let compilerPath = config.get<string>('compiler.path', '');
    const originalPath = compilerPath;

    if (!compilerPath) {
        compilerPath = 'threebx';  // Use from PATH
        log('  No compiler path configured, using "threebx" from PATH');
    } else {
        // Resolve VS Code variables that aren't automatically substituted in settings
        compilerPath = resolveVariables(compilerPath);
        log(`  Compiler path from settings: "${originalPath}"`);
        log(`  Resolved compiler path: "${compilerPath}"`);
    }

    lspOutputChannel.appendLine(`3BX Extension: Starting language server...`);
    lspOutputChannel.appendLine(`  Compiler path: ${compilerPath}`);
    lspOutputChannel.appendLine(`  Trace level: ${traceLevel}`);

    // Check if compiler file exists (if it's an absolute path)
    const fs = require('fs');
    const isAbsolutePath = compilerPath.startsWith('/') || /^[A-Za-z]:[\\/]/.test(compilerPath);
    log(`  Is absolute path: ${isAbsolutePath}`);

    if (isAbsolutePath) {
        const exists = fs.existsSync(compilerPath);
        log(`  File exists check: ${exists}`);

        if (!exists) {
            const errorMsg = `Compiler not found at: ${compilerPath}`;
            log(errorMsg, 'ERROR');
            log('  LSP features disabled. Please check 3bx.compiler.path setting.', 'ERROR');
            lspOutputChannel.appendLine(`  ERROR: ${errorMsg}`);
            lspOutputChannel.appendLine(`  LSP features disabled. Please check 3bx.compiler.path setting.`);
            lspOutputChannel.show();
            vscode.window.showWarningMessage(
                `3BX: Compiler not found at '${compilerPath}'. ` +
                'LSP features (go-to-definition, etc.) are disabled. ' +
                'Check Output > "3BX Extension" for details.'
            );
            return;
        }

        // Check if the file is executable
        try {
            fs.accessSync(compilerPath, fs.constants.X_OK);
            log('  File is executable: true');
        } catch {
            log('  File is executable: false (may cause issues on Unix)', 'WARN');
        }
    } else {
        // Try to find the compiler in PATH
        log('  Searching for compiler in PATH...');
        const { execSync } = require('child_process');
        try {
            const whichResult = execSync(`which ${compilerPath} 2>/dev/null || where ${compilerPath} 2>nul`, { encoding: 'utf8' }).trim();
            log(`  Found in PATH: ${whichResult}`);
        } catch {
            log('  Compiler not found in PATH', 'WARN');
            log('  LSP features may not work without a valid compiler path', 'WARN');
        }
    }

    // Create a separate output channel for server stderr logs
    const serverStderrChannel = vscode.window.createOutputChannel('3BX Language Server (stderr)');
    serverStderrChannel.appendLine('Server stderr output will appear here...');
    serverStderrChannel.show(true);  // Show the channel to help debugging

    // Server options - spawn the compiler with --lsp flag
    // Use spawn options to capture stderr for debugging
    const serverOptions: ServerOptions = () => {
        return new Promise((resolve, reject) => {
            const { spawn } = require('child_process');

            log('  Spawning server process: ' + compilerPath + ' --lsp');
            const serverProcess = spawn(compilerPath, ['--lsp'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                env: { ...process.env }
            });

            if (!serverProcess || !serverProcess.pid) {
                const errorMessage = 'Failed to spawn server process';
                log(errorMessage, 'ERROR');
                reject(new Error(errorMessage));
                return;
            }

            log('  Server process spawned with PID: ' + serverProcess.pid);

            // Capture stderr and forward to output channel
            serverProcess.stderr.on('data', (data: Buffer) => {
                const message = data.toString();
                serverStderrChannel.appendLine(message.trimEnd());
                log('  [Server stderr] ' + message.trimEnd(), 'DEBUG');
            });

            serverProcess.on('error', (error: Error) => {
                log('  Server process error: ' + error.message, 'ERROR');
                serverStderrChannel.appendLine('Server error: ' + error.message);
            });

            serverProcess.on('exit', (code: number | null, signal: string | null) => {
                log('  Server process exited with code ' + code + ', signal ' + signal);
                serverStderrChannel.appendLine('Server exited with code ' + code);
            });

            // Return the process streams for the LSP client to use
            const streamInfo: StreamInfo = {
                reader: serverProcess.stdout,
                writer: serverProcess.stdin
            };
            resolve(streamInfo);
        });
    };

    log('  Server options configured (using custom spawn to capture stderr)');

    // Client options
    const traceOutputChannel = vscode.window.createOutputChannel('3BX Language Server Trace');
    const clientOptions: LanguageClientOptions = {
        // Register the server for 3bx documents
        documentSelector: [{ scheme: 'file', language: '3bx' }],
        synchronize: {
            // Notify the server about file changes to .3bx files
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.3bx')
        },
        outputChannel: lspOutputChannel,
        traceOutputChannel: traceOutputChannel,
        initializationOptions: {
            // Pass any initialization options to the server
            trace: traceLevel
        }
    };

    log('  Client options configured');
    log('    Document selector: { scheme: "file", language: "3bx" }');

    // Create the language client
    client = new LanguageClient(
        '3bx',
        '3BX Language Server',
        serverOptions,
        clientOptions
    );

    // Add state change listener for debugging
    client.onDidChangeState((event) => {
        const stateNames: { [key: number]: string } = {
            [State.Stopped]: 'Stopped',
            [State.Starting]: 'Starting',
            [State.Running]: 'Running'
        };
        log(`  LSP client state changed: ${stateNames[event.oldState] || event.oldState} -> ${stateNames[event.newState] || event.newState}`);
    });

    try {
        log('  Calling client.start()...');
        lspOutputChannel.appendLine(`  Starting LSP client...`);

        // Start the client (also launches the server)
        await client.start();

        log('LSP client started successfully!', 'INFO');
        log('  Go-to-definition and other LSP features should now be available');
        lspOutputChannel.appendLine(`  LSP client started successfully!`);

        // Log server capabilities if available
        if (client.initializeResult) {
            log('  Server capabilities received:');
            const caps = client.initializeResult.capabilities;
            log(`    - definitionProvider: ${!!caps.definitionProvider}`);
            log(`    - hoverProvider: ${!!caps.hoverProvider}`);
            log(`    - completionProvider: ${!!caps.completionProvider}`);
            log(`    - referencesProvider: ${!!caps.referencesProvider}`);
            log(`    - documentSymbolProvider: ${!!caps.documentSymbolProvider}`);
        }
    } catch (error) {
        // Handle case where compiler doesn't exist or doesn't support --lsp
        const errorMessage = error instanceof Error ? error.message : String(error);
        const errorStack = error instanceof Error ? error.stack : undefined;

        log(`Failed to start LSP: ${errorMessage}`, 'ERROR');
        if (errorStack) {
            log(`  Stack trace: ${errorStack}`, 'ERROR');
        }
        lspOutputChannel.appendLine(`  ERROR: Failed to start LSP: ${errorMessage}`);
        lspOutputChannel.show();

        if (errorMessage.includes('ENOENT') || errorMessage.includes('spawn')) {
            const warningMsg = `3BX Language Server: Could not find compiler at '${compilerPath}'. ` +
                'Syntax highlighting will still work, but LSP features are disabled. ' +
                'Configure the compiler path in settings or install the 3BX compiler.';
            log(warningMsg, 'WARN');
            vscode.window.showWarningMessage(warningMsg);
        } else {
            const warningMsg = `3BX Language Server failed to start: ${errorMessage}. ` +
                'The compiler may not support --lsp yet. Syntax highlighting will still work.';
            log(warningMsg, 'WARN');
            vscode.window.showWarningMessage(warningMsg);
        }

        client = undefined;
    }
}

/**
 * Restarts the language server.
 */
async function restartServer(): Promise<void> {
    log('Restarting language server...');
    const config = vscode.workspace.getConfiguration('3bx');

    if (client) {
        log('  Stopping existing client...');
        await client.stop();
        client = undefined;
        log('  Existing client stopped');
    }

    if (config.get<boolean>('lsp.enabled', true)) {
        const context = getExtensionContext();
        if (context) {
            await startLanguageClient(context);
            vscode.window.showInformationMessage('3BX Language Server restarted');
            log('Language server restarted successfully');
        } else {
            log('Cannot restart: extension context not available', 'ERROR');
        }
    } else {
        log('LSP is disabled, not starting language client');
    }
}

/**
 * Gets the stored extension context.
 */
function getExtensionContext(): vscode.ExtensionContext | undefined {
    return extensionContext;
}

/**
 * Compiles the currently open 3BX file.
 */
async function compileCurrentFile(): Promise<void> {
    const editor = vscode.window.activeTextEditor;

    if (!editor) {
        vscode.window.showWarningMessage('No file is currently open');
        return;
    }

    if (editor.document.languageId !== '3bx') {
        vscode.window.showWarningMessage('Current file is not a 3BX file');
        return;
    }

    // Save the document first
    await editor.document.save();

    const config = vscode.workspace.getConfiguration('3bx');
    let compilerPath = config.get<string>('compiler.path', '');
    if (!compilerPath) {
        compilerPath = 'threebx';
    } else {
        compilerPath = resolveVariables(compilerPath);
    }

    const filePath = editor.document.uri.fsPath;

    // Create output channel for compiler output
    const outputChannel = vscode.window.createOutputChannel('3BX Compiler');
    outputChannel.show();
    outputChannel.appendLine(`Compiling: ${filePath}`);
    outputChannel.appendLine('---');

    // Run the compiler
    const { exec } = require('child_process');

    exec(`"${compilerPath}" "${filePath}"`, (error: Error | null, stdout: string, stderr: string) => {
        if (stdout) {
            outputChannel.appendLine(stdout);
        }
        if (stderr) {
            outputChannel.appendLine(stderr);
        }
        if (error) {
            outputChannel.appendLine(`\nCompilation failed with exit code: ${error.message}`);
            vscode.window.showErrorMessage('3BX compilation failed. See output for details.');
        } else {
            outputChannel.appendLine('\nCompilation successful!');
            vscode.window.showInformationMessage('3BX compilation successful!');
        }
    });
}
