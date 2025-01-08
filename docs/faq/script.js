async function loadFAQ() {
    try {
        const response = await fetch('faq.json');
        if (!response.ok) throw new Error('Failed to load FAQ data');
        const data = await response.json();

        document.getElementById('faqTitle').textContent = data.title;
        document.getElementById('faqDescription').textContent = data.description;

        const faqContent = document.getElementById('faqContent');
        data.categories.forEach(category => {
            const categorySection = document.createElement('div');
            categorySection.className = 'faq-category';
            
            categorySection.innerHTML = `
                <h2 class="category-title">${category.title}</h2>
                ${category.questions.map(q => `
                    <div class="faq-item">
                        <div class="faq-question">
                            <h3>${q.question}</h3>
                            <div class="faq-toggle"></div>
                        </div>
                        <div class="faq-answer">
                            <div class="faq-answer-content">${q.answer}</div>
                        </div>
                    </div>
                `).join('')}
            `;
            
            faqContent.appendChild(categorySection);
        });

        document.querySelectorAll('.faq-question').forEach(question => {
            question.addEventListener('click', () => {
                const faqItem = question.parentElement;
                faqItem.classList.toggle('active');
            });
        });

    } catch (error) {
        console.error('Error loading FAQ:', error);
        document.getElementById('faqContent').innerHTML = `
            <div class="error-message">
                Failed to load FAQ content. Please try refreshing the page.
            </div>
        `;
    }
}

document.addEventListener('DOMContentLoaded', loadFAQ);