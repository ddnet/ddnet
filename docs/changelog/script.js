const REPO_URL = 'https://api.github.com/repos/sjrc6/TaterClient-ddnet/releases';
const changelogElement = document.getElementById('changelog');

async function fetchReleases() {
    try {
        const response = await fetch(REPO_URL);
        if (!response.ok) {
            throw new Error('Failed to fetch releases');
        }
        const releases = await response.json();
        displayReleases(releases);
    } catch (error) {
        changelogElement.innerHTML = `
            <div class="error">
                Failed to load changelog. Please try again later.
            </div>
        `;
        console.error('Error fetching releases:', error);
    }
}

function formatDate(dateString) {
    return new Date(dateString).toLocaleDateString('en-US', {
        year: 'numeric',
        month: 'long',
        day: 'numeric'
    });
}

function displayReleases(releases) {
    const releasesHTML = releases.map(release => {
        let body = release.body || 'No description provided.';
        
        return `
            <div class="release">
                <div class="release-header">
                    <span class="release-version">${release.name || 'Unnamed Release'}</span>
                    <span class="release-date">${formatDate(release.published_at)}</span>
                </div>
                <div class="release-content">
                    ${marked.parse(body)}
                </div>
            </div>
        `;
    }).join('');

    changelogElement.innerHTML = releasesHTML;
}

fetchReleases();