-- Requires `PackageResolver` to be installed in the host application.
local selected = lsatie.invoke("package.resolve", {
  "satie=1.0.0",
  "satie=1.2.0",
  "dsl=0.4.0"
})
print(selected)
