const button = document.getElementById("click-me-btn");
let blue_background = false;
button.onclick = () => {
  if (blue_background) document.body.style.backgroundColor = "white";
  else document.body.style.backgroundColor = "#5e9cff";
  blue_background = !blue_background;
};
