# Close SuperglideTrainer.exe if running
Stop-Process -Name "SuperglideTrainer" -ErrorAction SilentlyContinue

cmake ..

cmake --build . --config Release

.\SuperglideTrainer.exe