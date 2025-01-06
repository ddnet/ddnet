document.addEventListener('DOMContentLoaded', () => {
    const versionElements = document.querySelectorAll('.version');
    
    async function fetchLatestVersion() {
        try {
            const response = await fetch('https://api.github.com/repos/sjrc6/TaterClient-ddnet/releases/latest');
            
            if (!response.ok) {
                throw new Error(`Network response was not ok: ${response.statusText}`);
            }
            
            const data = await response.json();
            const versionMatch = data.tag_name.match(/v?(\d+\.\d+\.\d+)/);
            
            const latestVersion = versionMatch ? versionMatch[1] : data.tag_name;
            
            versionElements.forEach(element => {
                element.textContent = `Latest: ${latestVersion}`;
            });
        } catch (error) {
            console.error('Error fetching version:', error);
            versionElements.forEach(element => {
                element.textContent = 'Latest: Unknown';
            });
        }
    }
    
    fetchLatestVersion();
});