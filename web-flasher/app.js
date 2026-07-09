const installButton = document.querySelector('esp-web-install-button');
const manifestLink = document.querySelector('#manifest-link');
const manifestStatus = document.querySelector('#manifest-status');

const params = new URLSearchParams(window.location.search);
const manifest = params.get('manifest') || './manifest.json';

installButton?.setAttribute('manifest', manifest);

if (manifestLink) {
  manifestLink.setAttribute('href', manifest);
}

if (manifestStatus) {
  manifestStatus.textContent = `${manifest} を使用します。`;
}

document.documentElement.dataset.webSerial = 'serial' in navigator ? 'supported' : 'unsupported';
