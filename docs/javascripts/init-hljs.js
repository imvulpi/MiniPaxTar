document.addEventListener("DOMContentLoaded", function () {
  hljs.highlightAll();
  document.querySelectorAll("pre code").forEach((codeBlock) => {
    const pre = codeBlock.parentNode;
    const copyButton = document.createElement("button");
    copyButton.className = "code-copy-btn";
    copyButton.type = "button";
    copyButton.innerText = "Copy";

    copyButton.addEventListener("click", () => {
      navigator.clipboard.writeText(codeBlock.innerText).then(() => {
        copyButton.innerText = "Copied!";
        copyButton.classList.add("copied");

        setTimeout(() => {
          copyButton.innerText = "Copy";
          copyButton.classList.remove("copied");
        }, 2000);
      });
    });

    pre.style.position = "relative";
    pre.appendChild(copyButton);
  });
});