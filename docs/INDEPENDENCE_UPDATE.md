# 依存関係更新方法

vscode内のc言語デバッグ用依存関係を更新するのには以下のコマンドを打ちます。\
※通常、task.jsonによってレポジトリ起動時に実行されますが、依存関係が更新されない場合は以下を実行してください

```bash:PowerShell
powershell -ExecutionPolicy Bypass -File tools/update_compile_commands.ps1
```
