/**
 * Upload a WAV file for indexing
 * @param {File} file - WAV file to upload
 * @param {Function} handleJsonResponse - Callback for handling response
 * @param {Function} setLoading - Callback for loading state
 */
export async function uploadFile(file, handleJsonResponse, setLoading) {
  if (!file) {
    throw new Error('No file provided');
  }
  
  const form = new FormData();
  form.append('file', file, file.name);
  
  try {
    setLoading(true);
    const resp = await fetch('/search_by_file', { method: 'POST', body: form });
    
    if (!resp.ok) {
      const errorText = await resp.text();
      throw new Error('Server error: ' + resp.status + '\n' + errorText);
    }
    
    const result = await resp.json();
    await handleJsonResponse(result, result.indexBase64);
  } catch (error) {
    console.error(error);
    throw error; // Re-throw to let caller handle
  } finally {
    setLoading(false);
  }
}
