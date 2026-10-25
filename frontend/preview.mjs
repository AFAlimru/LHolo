import { context } from 'esbuild';
import { copyFile, mkdir, readFile } from 'node:fs/promises';
import { watch } from 'node:fs';
import { fileURLToPath } from 'node:url';

process.chdir(fileURLToPath(new URL('.', import.meta.url)));
await mkdir('.preview', { recursive: true });
const copyHtml = () => copyFile('preview/index.html', '.preview/index.html');
await copyHtml();
const ctx = await context({
  entryPoints: ['preview/main.tsx'], outdir: '.preview', bundle: true,
  format: 'iife', target: 'es2019', sourcemap: true, logLevel: 'info',
  plugins: [{ name: 'preview-font', setup(build) {
    build.onLoad({ filter: /[\\/]src[\\/]styles[\\/]menu\.css$/ }, async args => ({
      // The browser preview needs no copyrighted game files or game install.
      contents: (await readFile(args.path, 'utf8')).replace(/@font-face\s*\{[^}]*\}/g, ''),
      loader: 'css', watchFiles: [args.path],
    }));
  } }],
});
if (process.argv.includes('--build')) { await ctx.rebuild(); await ctx.dispose(); }
else {
  await ctx.watch();
  const server = await ctx.serve({ servedir: '.preview', host: '127.0.0.1', port: Number(process.env.LHOLO_PREVIEW_PORT || 4173) });
  console.log(`LHolo browser preview: http://127.0.0.1:${server.port}`);
  console.log('Mock data only. Edit frontend sources to rebuild and refresh automatically. Ctrl+C stops the server.');
  const htmlWatcher = watch('preview/index.html', () => { void copyHtml(); });
  const stop = async () => { htmlWatcher.close(); await ctx.dispose(); process.exit(0); };
  process.on('SIGINT', stop); process.on('SIGTERM', stop);
}
