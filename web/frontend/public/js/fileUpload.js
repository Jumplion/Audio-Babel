export async function uploadFile(file, handleJsonResponse, setLoading) {
  if (!file) throw new Error('No file provided');
  const form = new FormData();
  form.append('file', file, file.name);
  try {
    setLoading(true);
    const resp = await fetch('/search_by_file', { method: 'POST', body: form });
    if (!resp.ok) {
      const txt = await resp.text();
      throw new Error('Server error: ' + resp.status + '\n' + txt);
    }
    const j = await resp.json();
    await handleJsonResponse(j, j.indexBase64);
  } finally {
    setLoading(false);
  }
}
