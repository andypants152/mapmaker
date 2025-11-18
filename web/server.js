const express = require("express");
const path = require("path");

const PORT = process.env.PORT || 8123;
const app = express();
const publicDir = path.join(__dirname, "public");

// Serve prebuilt WebAssembly output and static assets.
app.use(express.static(publicDir, {
    setHeaders: (res, filePath) => {
        if (path.extname(filePath) === ".wasm") {
            res.type("application/wasm");
        }
    },
}));

// Fallback to index.html for any unknown routes so we can deep link assets.
app.use((req, res) => {
    res.sendFile(path.join(publicDir, "index.html"));
});

app.listen(PORT, () => {
    console.log(`Mapmaker dev server running at http://localhost:${PORT}`);
});
