# 独立評価プロジェクト

技術項目ごとに、別のe2 studioプロジェクトを置くディレクトリです。

## 規則

- 一つの評価では一つの技術的な疑問を扱う
- `firmware/ra8p1/`を変更しない
- 周辺回路設定と生成コードを評価ごとに分離する
- 実機結果を`docs/EXPERIMENTS.md`へ記録する
- 確認済みの機能だけを統合システムへ移す

## 新規作成

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools/New-EvaluationProject.ps1 -Name evaluation_name
```

プロジェクト名には小文字の英字、数字、下線だけを使用します。
