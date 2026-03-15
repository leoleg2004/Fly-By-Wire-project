# Expand Document Section

Espandi una singola sezione di un documento Markdown con contenuto tecnico rigoroso, diagrammi Mermaid, e riferimenti incrociati.

## Workflow

1. **Leggi il file Markdown** target
2. **Identifica la sezione** da espandere (titolo esatto)
3. **Aggiungi contenuto tecnico dettagliato**:
   - Equazioni in LaTeX (`$...$` o `$$...$$`)
   - Spiegazione concettuale
   - Implementazione pratica
4. **Includi Mermaid diagrams** dove l'architettura è descritta
5. **Riferimenti incrociati**: Stevens & Lewis, NASA docs, MATLAB
6. **Limita l'edit a <200 linee** per evitare API limits
7. **Usa Edit tool**, non full file rewrites

## Esempio

```
/expand-section
target: docs/technical.md
section: "RK4 Integration"
content: "Detailed derivation of RK4 with F-16 dynamics, including Butcher tableau and local truncation error. Add Mermaid flowchart."
```

## Output

- Mostra il diff delle modifiche
- Conferma linee aggiunte vs limite
- Elenca fonti citate
