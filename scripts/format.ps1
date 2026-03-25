Get-ChildItem -Recurse -Path NPTracerPlugin, NPTracerRenderer `
    -Include *.h, *.cpp, *.inl | 
    ForEach-Object { clang-format -i $_.FullName}
