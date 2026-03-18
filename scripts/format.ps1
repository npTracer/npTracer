Get-ChildItem -Recurse -Path src `
    -Include *.h, *.cpp, *.inl | 
    ForEach-Object { clang-format -i $_.FullName}
