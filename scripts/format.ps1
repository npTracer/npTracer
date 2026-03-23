Get-ChildItem -Recurse -Path NPTracerPlugin `
    -Include *.h, *.cpp, *.inl | 
    ForEach-Object { clang-format -i $_.FullName}
