# Dev Server

## Check what's running

```bash
# Find processes using port 5173 (Vite default)
lsof -i :5173
```

## Kill a running server

```bash
# Kill whatever is on port 5173
kill $(lsof -ti :5173)

# Force kill if it won't stop
kill -9 $(lsof -ti :5173)
```

## Start a new server

```bash
cd web
npm run dev
```

Runs on http://localhost:5173 by default.
